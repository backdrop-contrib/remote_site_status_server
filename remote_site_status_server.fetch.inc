<?php

/**
 * @file
 * Authoritative latest-version check for Remote Site Status Server.
 *
 * Walks the fleet's distinct project inventory, fetches each project's
 * release-history XML from the Backdrop update server, parses the latest
 * stable and latest security releases, and stores them on the project row.
 * Trigger is manual only — a "Fetch latest versions" button — so the operator
 * gets current data at the point of need without hammering the update server
 * on a fixed schedule.
 */

/**
 * The Backdrop release-history feed base URL.
 *
 * Mirrors UPDATE_DEFAULT_URL from the core Update module, defined here so
 * this file does not depend on that module being enabled.
 */
define('REMOTE_SITE_STATUS_UPDATE_URL', 'https://updates.backdropcms.org/release-history');

/**
 * Page callback: Projects tab — fetch form followed by the projects list view.
 *
 * Renders the fetch-trigger form and the projects list view as separate
 * siblings so the view's exposed filters have their own <form> element and do
 * not accidentally submit the fetch form when the operator clicks Apply.
 *
 * @return array
 *   A render array containing the fetch form and the embedded view.
 */
function remote_site_status_server_projects_page() {
  $build['fetch_form'] = backdrop_get_form(
    'remote_site_status_server_fetch_updates_form'
  );
  $view_output = views_embed_view('remote_site_status_projects_list');
  if ($view_output !== NULL) {
    $build['projects_list'] = array(
      '#markup' => $view_output,
    );
  }
  return $build;
}

/**
 * Page callback / form builder: manual latest-version fetch trigger.
 *
 * Displays the last-check timestamp (if any) and a button that starts a
 * batched fetch of the Backdrop release-history feed for every project in
 * the fleet inventory. Accessible at
 * admin/config/remote-site-status-server/projects.
 *
 * @param array $form
 *   The form array.
 * @param array $_form_state
 *   The form state array (unused; required by the Form API signature).
 *
 * @return array
 *   The form definition.
 */
function remote_site_status_server_fetch_updates_form($form, &$_form_state) {
  $last_fetch = state_get('remote_site_status_server_last_fetch', NULL);

  if ($last_fetch) {
    $form['last_checked'] = array(
      '#type' => 'item',
      '#markup' => '<p>' . t('Project versions last checked <strong>@ago ago</strong>.', array(
        '@ago' => format_interval(REQUEST_TIME - $last_fetch),
      )) . '</p>',
    );
  }
  else {
    $form['last_checked'] = array(
      '#type' => 'item',
      '#markup' => '<p>' . t('Project versions have not been checked yet.') . '</p>',
    );
  }

  $count = (int) db_select('remote_site_status_project', 'p')
    ->countQuery()
    ->execute()
    ->fetchField();

  if ($count === 0) {
    $form['notice'] = array(
      '#type' => 'item',
      '#markup' => '<p>' . t('No projects are in the fleet inventory yet. Once client sites report in, their installed projects will appear here.') . '</p>',
    );
    return $form;
  }

  $unchecked = (int) db_select('remote_site_status_project', 'p')
    ->isNull('p.latest_checked_at')
    ->countQuery()
    ->execute()
    ->fetchField();

  if ($unchecked > 0) {
    $form['unchecked_notice'] = array(
      '#type' => 'item',
      '#markup' => '<div class="messages warning">' . format_plural($unchecked,
        '1 project has been added to the inventory and has not yet been checked against the Backdrop release feed.',
        '@count projects have been added to the inventory and have not yet been checked against the Backdrop release feed.'
      ) . ' ' . t('Run <em>Fetch latest versions</em> below to retrieve their release data. Custom or private projects not listed on Backdrop.org will remain without version data after the check — this is expected.') . '</div>',
    );
  }

  $form['description'] = array(
    '#type' => 'item',
    '#markup' => '<p>' . t('Fetch the latest stable and security release for each of the <strong>@count</strong> projects currently in the fleet inventory. One request is made per project — for large inventories this may take a minute.', array('@count' => $count)) . '</p>',
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Fetch latest versions'),
  );

  return $form;
}

/**
 * Submit handler: start the batched latest-version fetch.
 *
 * Loads all distinct project rows, creates one batch operation per project,
 * and hands off to the Batch API. The batch redirects to the admin overview
 * on completion so the operator can see the updated "last checked" time.
 *
 * @param array $_form
 *   The form array (unused; required by the Form API signature).
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_fetch_updates_form_submit($_form,
    &$form_state) {
  $projects = db_select('remote_site_status_project', 'p')
    ->fields('p', array('id', 'project', 'type'))
    ->orderBy('p.project')
    ->execute();

  $operations = array();
  foreach ($projects as $project) {
    $operations[] = array(
      'remote_site_status_server_fetch_batch_operation',
      array($project->id, $project->project, $project->type),
    );
  }

  if (empty($operations)) {
    backdrop_set_message(t('No projects in the fleet inventory to check.'), 'warning');
    $form_state['redirect'] = 'admin/config/remote-site-status-server';
    return;
  }

  $batch = array(
    'title' => t('Fetching latest project versions'),
    'operations' => $operations,
    'finished' => 'remote_site_status_server_fetch_batch_finished',
    'file' => backdrop_get_path('module', 'remote_site_status_server')
      . '/remote_site_status_server.fetch.inc',
    'progress_message' => t('Checking @current of @total projects...'),
    'error_message' => t('An error occurred while fetching version data.'),
  );

  batch_set($batch);
  // After batch completion, return to the overview so the "last checked"
  // timestamp is visible.
  $form_state['redirect'] = 'admin/config/remote-site-status-server';
}

/**
 * Batch operation: fetch and store the latest release data for one project.
 *
 * Fetches the project's release-history XML, parses the latest stable and
 * latest security versions, updates the project row, and derives the new
 * security_status (needs_review / reviewed) from whether the fetched security
 * version is newer than the operator's last-reviewed version.
 *
 * Projects that return no feed (HTTP 404/501, custom modules with no upstream)
 * have their latest_checked_at updated but latest_version stays NULL so they
 * are shown inventory-only and never falsely flagged out of date.
 *
 * @param int $project_id
 *   The remote_site_status_project row id.
 * @param string $project
 *   The project machine name.
 * @param string $type
 *   The project type (module, theme, layout).
 * @param array $context
 *   Batch API context array (passed by reference).
 */
function remote_site_status_server_fetch_batch_operation($project_id,
    $project, $type, &$context) {
  $context['message'] = t('Fetching release data for @project (@type)...', array(
    '@project' => $project,
    '@type' => $type,
  ));

  if (!isset($context['results']['updated'])) {
    $context['results']['updated'] = 0;
    $context['results']['no_upstream'] = 0;
  }

  $releases = remote_site_status_server_fetch_project_releases($project);

  // Always record that we attempted a check for this project.
  $fields = array('latest_checked_at' => time());

  if ($releases !== FALSE) {
    $fields['latest_version'] = $releases['latest_version'];
    $fields['latest_security_version'] = $releases['latest_security_version'];
    $fields['project_url'] = $releases['project_url'];

    // Derive security_status.
    //
    // If a security release exists but has been superseded by a newer stable
    // release, the fix is already in the latest version. In that case we only
    // flag needs_review when fleet sites are still running a version older than
    // the security release — concern is moot and we clear the flag.
    //
    // If the security release is the latest (or newer than latest), we use the
    // operator-review flow: needs_review until the operator marks it reviewed.
    if (!empty($releases['latest_security_version'])) {
      $existing = db_select('remote_site_status_project', 'p')
        ->fields('p', array('security_reviewed_version', 'security_notes'))
        ->condition('id', $project_id)
        ->execute()
        ->fetchObject();

      $security_ver = $releases['latest_security_version'];
      $latest_ver = $releases['latest_version'];

      $sec_superseded = !empty($latest_ver) &&
        remote_site_status_server_compare_versions(
          $security_ver, $latest_ver
        ) < 0;

      if ($sec_superseded) {
        // Security fix is baked into a newer stable release. Flag only if the
        // fleet has sites still running a vulnerable version.
        $installed = db_select('remote_site_status_site_project', 'sp')
          ->fields('sp', array('installed_version'))
          ->condition('project_id', $project_id)
          ->isNotNull('installed_version')
          ->execute()
          ->fetchCol();

        $vulnerable = FALSE;
        foreach ($installed as $v) {
          if (remote_site_status_server_compare_versions(
              $v, $security_ver) < 0) {
            $vulnerable = TRUE;
            break;
          }
        }
        $fields['security_status'] = $vulnerable ? 'Needs review' : NULL;
      }
      else {
        // Security release is at or beyond the latest stable — operator must
        // review and acknowledge.
        $reviewed = $existing ? $existing->security_reviewed_version : NULL;

        if (empty($reviewed) || remote_site_status_server_compare_versions(
            $security_ver, $reviewed) > 0) {
          $fields['security_status'] = 'Needs review';

          // Prepend a dated stub to security_notes when the toggle is on.
          $settings = config('remote_site_status_server.settings');
          if ($settings->get('security_notes_stub')) {
            $date_format = $settings->get('security_notes_date_format')
              ?: 'short';
            $stub = format_date(time(), $date_format)
              . ' - ' . $security_ver . ' - ';
            $current_notes = $existing
              ? (string) $existing->security_notes : '';
            $fields['security_notes'] = trim($stub . "\n" . $current_notes);
          }
        }
        else {
          $fields['security_status'] = 'Reviewed';
        }
      }
    }
    else {
      // No security releases known for this project.
      $fields['security_status'] = NULL;
    }

    $context['results']['updated']++;
  }
  else {
    // No upstream feed or network error: leave latest_* as-is (NULL on first
    // check, preserved on re-check) and treat as inventory-only.
    $context['results']['no_upstream']++;
  }

  db_update('remote_site_status_project')
    ->fields($fields)
    ->condition('id', $project_id)
    ->execute();
}

/**
 * Batch finished callback: report results and record the check timestamp.
 *
 * @param bool $success
 *   TRUE if the batch completed without a fatal error.
 * @param array $results
 *   Aggregated results from the batch operations.
 * @param array $_operations
 *   Remaining operations if $success is FALSE (unused on success path).
 */
function remote_site_status_server_fetch_batch_finished($success, $results,
    $_operations) {
  if ($success) {
    $updated = $results['updated'] ?? 0;
    $no_upstream = $results['no_upstream'] ?? 0;

    backdrop_set_message(t('Latest versions fetched for @updated project(s). @no_upstream had no upstream feed and remain inventory-only.', array(
      '@updated' => $updated,
      '@no_upstream' => $no_upstream,
    )));

    // Record the global last-fetch time so the admin page can show it.
    state_set('remote_site_status_server_last_fetch', time());
  }
  else {
    backdrop_set_message(t('An error occurred while fetching version data. Partial results may have been saved.'), 'error');
  }
}

/**
 * Fetch and parse the release-history XML for one project.
 *
 * Makes one HTTP GET to the Backdrop update server for this project's feed,
 * parses the XML, and returns the latest stable and latest security version
 * strings. Returns FALSE when the feed is unavailable (no upstream, 404, 501,
 * network error) so the caller can treat the project as inventory-only.
 *
 * @param string $project
 *   The project machine name (e.g. 'views', 'webform').
 *
 * @return array|false
 *   Associative array with keys 'latest_version' and 'latest_security_version'
 *   (either may be NULL if no matching release exists), or FALSE on failure.
 */
function remote_site_status_server_fetch_project_releases($project) {
  $url = REMOTE_SITE_STATUS_UPDATE_URL . '/' . rawurlencode($project)
    . '/' . BACKDROP_CORE_COMPATIBILITY;

  $response = backdrop_http_request($url, array('timeout' => 20));
  $code = isset($response->code) ? (int) $response->code : 0;

  if (isset($response->error)) {
    watchdog('remote_site_status_server',
      'Failed to fetch release history for @project: @error', array(
      '@project' => $project,
      '@error' => $response->error,
    ), WATCHDOG_WARNING);
    return FALSE;
  }

  // 501 means the update server has no record of this project. 404 similarly.
  // Anything other than 200 is treated as "no upstream".
  if ($code !== 200 || empty($response->data)) {
    return FALSE;
  }

  $data = remote_site_status_server_parse_release_xml($response->data);

  if (!$data || empty($data['releases'])) {
    return FALSE;
  }

  $latest_version = NULL;
  $latest_any = NULL;
  $latest_security_version = NULL;

  // Releases in the feed are ordered newest-first. Walk to find the first
  // stable release and the first security release.
  foreach ($data['releases'] as $version => $release) {
    if (!isset($release['status']) || $release['status'] !== 'published') {
      continue;
    }

    // Track the first published release of any stability (beta, rc, etc.) as a
    // fallback for projects that have no stable release yet.
    if ($latest_any === NULL) {
      $latest_any = (string) $version;
    }

    // Latest stable: no version_extra suffix (no -beta, -rc, -preview, etc.).
    if ($latest_version === NULL && empty($release['version_extra'])) {
      $latest_version = (string) $version;
    }

    // Latest security: first published release with the 'Security update' term.
    if ($latest_security_version === NULL
        && isset($release['terms']['Release type'])
        && in_array('Security update', $release['terms']['Release type'])) {
      $latest_security_version = (string) $version;
    }

    if ($latest_version !== NULL && $latest_security_version !== NULL) {
      break;
    }
  }

  // Fall back to the latest pre-release when no stable release exists yet.
  if ($latest_version === NULL) {
    $latest_version = $latest_any;
  }

  return array(
    'latest_version' => $latest_version,
    'latest_security_version' => $latest_security_version,
    'project_url' => !empty($data['link']) ? (string) $data['link'] : NULL,
  );
}

/**
 * Parse the XML of a Backdrop release-history feed for one project.
 *
 * Adapted from core's update_parse_xml() (update.fetch.inc) so this module
 * does not depend on the Update Manager module being enabled. Returns the same
 * structure: an array keyed by short_name info and a 'releases' sub-array
 * keyed by version string.
 *
 * @param string $raw_xml
 *   The raw XML string returned by the release-history feed.
 *
 * @return array|null
 *   Parsed release data, or NULL if the XML is invalid or incomplete.
 */
function remote_site_status_server_parse_release_xml($raw_xml) {
  try {
    $xml = new SimpleXMLElement($raw_xml);
  }
  catch (Exception $_e) {
    return NULL;
  }

  if (!isset($xml->short_name)) {
    return NULL;
  }

  $data = array();
  foreach ($xml as $k => $v) {
    $data[$k] = (string) $v;
  }

  $data['releases'] = array();
  if (isset($xml->releases)) {
    foreach ($xml->releases->children() as $release) {
      $version = (string) $release->version;
      $data['releases'][$version] = array();

      foreach ($release->children() as $k => $v) {
        $data['releases'][$version][$k] = (string) $v;
      }

      $data['releases'][$version]['terms'] = array();
      if ($release->terms) {
        foreach ($release->terms->children() as $term) {
          $term_name = (string) $term->name;
          if (!isset($data['releases'][$version]['terms'][$term_name])) {
            $data['releases'][$version]['terms'][$term_name] = array();
          }
          $data['releases'][$version]['terms'][$term_name][]
            = (string) $term->value;
        }
      }
    }
  }

  return $data;
}

/**
 * Compare two Backdrop version strings.
 *
 * Backdrop versions carry a core-compat prefix (e.g. '1.x-') before the
 * semantic version number. Native string comparison (strcmp, <, >) gives the
 * wrong result for versions like 1.x-1.9.0 vs 1.x-1.10.0. This strips the
 * prefix and delegates to PHP's version_compare() which handles numeric parts
 * and pre-release suffixes (-beta, -rc, -preview) correctly.
 *
 * Load-bearing in two places: picking the latest release from a feed, and the
 * installed-vs-latest comparison (Views filter handlers).
 *
 * @param string $a
 *   A Backdrop version string, e.g. '1.x-1.10.0' or '1.x-2.0.0-beta1'.
 * @param string $b
 *   A Backdrop version string to compare against.
 *
 * @return int
 *   -1 if $a < $b, 0 if equal, 1 if $a > $b.
 */
function remote_site_status_server_compare_versions($a, $b) {
  $clean_a = preg_replace('/^\d+\.x-/', '', (string) $a);
  $clean_b = preg_replace('/^\d+\.x-/', '', (string) $b);
  return version_compare($clean_a, $clean_b);
}
