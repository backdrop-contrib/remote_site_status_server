<?php

/**
 * @file
 * Admin UI for Remote Site Status Server: site registration and the
 * key-issuance flow (generate + hash + show once, regenerate, revoke, delete).
 */

/**
 * Page callback: module landing page showing the registered-sites View.
 *
 * The site list is rendered as an embedded View so the operator gets paging,
 * sorting and filtering out of the box and can customise the display. Admin
 * action links (Edit / Revoke / Delete) are fields in the View itself.
 *
 * @return string
 *   Rendered HTML: last-fetch status line followed by the embedded View.
 */
function remote_site_status_server_sites_page() {
  $last_fetch = state_get('remote_site_status_server_last_fetch', NULL);
  if ($last_fetch) {
    $ago = format_interval(REQUEST_TIME - $last_fetch);
    $output = '<p class="rstat-last-fetch">'
      . t('Project versions last checked <strong>@ago ago</strong>.',
        array('@ago' => $ago))
      . '</p>';
  }
  else {
    $output = '<p class="rstat-last-fetch">'
      . t('Project versions have not been checked yet.')
      . '</p>';
  }

  $output .= views_embed_view('remote_site_status_site_list');
  return $output;
}

/**
 * Load a site row by id, or redirect to the overview if it does not exist.
 *
 * Centralises the "not found" handling shared by the edit, revoke and delete
 * forms so each callback does not repeat it.
 *
 * @param int $site_id
 *   The site id.
 *
 * @return object
 *   The site row. Does not return if the site is missing (redirects instead).
 */
function remote_site_status_server_site_load_or_404($site_id) {
  $site = db_select('remote_site_status_site', 's')
    ->fields('s')
    ->condition('id', $site_id)
    ->execute()
    ->fetchObject();

  if (!$site) {
    backdrop_set_message(t('Site not found.'), 'error');
    backdrop_goto('admin/config/remote-site-status-server');
  }

  return $site;
}

/**
 * Stash a freshly minted key for one-time display on the edit form.
 *
 * The plaintext key is never stored in the database, so it is held in the
 * session only long enough to survive the post-save redirect, after which the
 * edit form renders it once and clears it. Rendering it in the form (rather
 * than a status message) keeps the Copy button in normal form DOM so its
 * behaviour binds reliably. Shared by the add and regenerate flows.
 *
 * @param int $site_id
 *   The site the key belongs to.
 * @param string $key
 *   The plaintext API key.
 */
function remote_site_status_server_stash_new_key($site_id, $key) {
  $_SESSION['remote_site_status_server_new_key'] = array(
    'site_id' => (int) $site_id,
    'key' => $key,
  );
}

/**
 * Form builder: register a site (add) or edit an existing one.
 *
 * One form handles both. On add it collects label and URL and mints a key on
 * save. On edit it never re-displays the key but offers a Regenerate action,
 * plus links to revoke/reactivate and delete. The key is the site's identity,
 * so it is generated and hashed in the submit handler, never entered by hand.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 * @param int|null $site_id
 *   The site id when editing, NULL when adding.
 *
 * @return array
 *   The form definition.
 */
function remote_site_status_server_site_form($form, &$form_state,
    $site_id = NULL) {
  $site = NULL;
  if (!empty($site_id)) {
    $site = remote_site_status_server_site_load_or_404($site_id);
  }

  $form['#site_id'] = $site_id;

  // The show-once key message lands on this page after add/regenerate; load the
  // Copy-button behaviour and its styling here so they are present.
  $module_path = backdrop_get_path('module', 'remote_site_status_server');
  backdrop_add_css($module_path . '/css/remote_site_status_server.admin.css');
  backdrop_add_js($module_path . '/js/remote_site_status_server.admin.js');

  // One-time display of a freshly generated key, stashed by the add/regenerate
  // flow to survive the redirect. Shown once, then cleared from the session.
  $session_key = $_SESSION['remote_site_status_server_new_key'] ?? NULL;
  if ($site && $session_key && $session_key['site_id'] == $site_id) {
    $new_key = $session_key['key'];
    unset($_SESSION['remote_site_status_server_new_key']);

    $form['new_key'] = array(
      '#type' => 'fieldset',
      '#title' => t('Site saved — copy your new API key'),
      '#attributes' => array('class' => array('rstat-new-key')),
      '#weight' => -10,
    );
    $form['new_key']['key'] = array(
      '#type' => 'item',
      '#description' => t('Copy this key now and store it with the client site. It is shown only once and cannot be retrieved later.'),
      '#markup' => '<div class="rstat-key-copy">'
        . '<input type="text" class="rstat-key-value" value="'
        . check_plain($new_key) . '" readonly="readonly" size="64" />'
        . '<button type="button" class="button rstat-copy-key">'
        . icon('copy', array('alt' => t('Copy to clipboard')))
        . '<span class="rstat-copy-label">' . t('Copy') . '</span>'
        . '</button>'
        . '</div>',
    );
  }

  $form['label'] = array(
    '#type' => 'textfield',
    '#title' => t('Label'),
    '#description' => t('An operator-facing name for this site. Doubles as an optional grouping label, e.g. "Acme Ltd — production".'),
    '#default_value' => $site ? $site->label : '',
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  $form['url'] = array(
    '#type' => 'textfield',
    '#title' => t('Site URL'),
    '#description' => t('The full site URL, e.g. https://acme.co.uk. HTTPS is expected.'),
    '#default_value' => $site ? $site->url : '',
    '#required' => TRUE,
    '#maxlength' => 255,
  );

  if ($site) {
    // Edit mode: show current state and the key-related actions. The key
    // itself is never re-displayed — only regeneration can surface a new one.
    $status_label = ($site->status === 'revoked') ? t('Revoked') : t('Active');
    $form['status_display'] = array(
      '#type' => 'item',
      '#title' => t('Status'),
      '#markup' => check_plain($status_label),
    );

    $form['regenerate_key'] = array(
      '#type' => 'submit',
      '#value' => t('Regenerate key'),
      '#submit' => array('remote_site_status_server_site_regenerate_submit'),
      '#limit_validation_errors' => array(),
      '#weight' => 1,
    );

    $base = 'admin/config/remote-site-status-server/sites/' . $site_id;
    $revoke_text = ($site->status === 'revoked')
      ? t('Reactivate this site') : t('Revoke this site');
    $form['actions_links'] = array(
      '#type' => 'item',
      '#markup' => l($revoke_text, $base . '/revoke')
        . ' &nbsp;|&nbsp; '
        . l(t('Delete this site'), $base . '/delete'),
      '#weight' => 20,
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => $site_id ? t('Save site') : t('Register site'),
    '#weight' => 10,
  );

  return $form;
}

/**
 * Validate handler for the site add/edit form.
 *
 * Enforces a unique URL and warns (does not block) when the URL is not HTTPS,
 * since the aggregated data warrants an encrypted reporting channel.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_site_form_validate($form, &$form_state) {
  $url = trim($form_state['values']['url']);
  $site_id = $form['#site_id'];

  $query = db_select('remote_site_status_site', 's')
    ->fields('s', array('id'))
    ->condition('url', $url);
  if (!empty($site_id)) {
    $query->condition('id', $site_id, '<>');
  }
  if ($query->execute()->fetchField()) {
    form_set_error('url', t('A site with this URL is already registered.'));
  }

  if (stripos($url, 'https://') !== 0) {
    backdrop_set_message(t('This site URL is not HTTPS. Reports carry an inventory of your estate and should travel over an encrypted channel.'), 'warning');
  }
}

/**
 * Submit handler for the site add/edit form.
 *
 * On add, mints a key, stores only its hash, and shows the plaintext once. On
 * edit, saves the label/URL only; the key is untouched here (see regenerate).
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_site_form_submit($form, &$form_state) {
  $site_id = $form['#site_id'];

  $fields = array(
    'label' => trim($form_state['values']['label']),
    'url' => trim($form_state['values']['url']),
  );

  if ($site_id) {
    db_update('remote_site_status_site')
      ->fields($fields)
      ->condition('id', $site_id)
      ->execute();
    backdrop_set_message(t('Site updated.'));
    // Saving returns to the site list; only Regenerate key stays on the edit
    // form (so the freshly shown key is not immediately navigated away from).
    $form_state['redirect'] = 'admin/config/remote-site-status-server';
  }
  else {
    $key = remote_site_status_server_generate_key();
    $fields['key_hash'] = remote_site_status_server_hash_key($key);
    $fields['status'] = 'active';
    $new_id = db_insert('remote_site_status_site')
      ->fields($fields)
      ->execute();
    backdrop_set_message(t('Site registered.'));
    remote_site_status_server_stash_new_key($new_id, $key);
    $form_state['redirect'] = 'admin/config/remote-site-status-server/sites/'
      . $new_id . '/edit';
  }
}

/**
 * Submit handler: regenerate a site's API key.
 *
 * Mints a fresh key, replaces the stored hash, and shows the new plaintext
 * once. The old key stops working immediately; the site's history is kept.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_site_regenerate_submit($form, &$form_state) {
  $site_id = $form['#site_id'];

  $key = remote_site_status_server_generate_key();
  db_update('remote_site_status_site')
    ->fields(array('key_hash' => remote_site_status_server_hash_key($key)))
    ->condition('id', $site_id)
    ->execute();

  backdrop_set_message(t('A new API key has been generated. The previous key no longer works.'));
  remote_site_status_server_stash_new_key($site_id, $key);
  $form_state['redirect'] = 'admin/config/remote-site-status-server/sites/'
    . $site_id . '/edit';
}

/**
 * Confirm form: revoke or reactivate a site.
 *
 * Revoking marks the site inactive so its key can no longer authenticate,
 * while keeping its reported history; reactivating restores authentication.
 * One-click offboarding the operator can link from the listing View.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 * @param int $site_id
 *   The site id.
 *
 * @return array
 *   The confirm form.
 */
function remote_site_status_server_site_revoke_form($form, &$form_state,
    $site_id) {
  $site = remote_site_status_server_site_load_or_404($site_id);
  $form['#site_id'] = $site_id;
  $form['#reactivate'] = ($site->status === 'revoked');

  if ($form['#reactivate']) {
    $question = t('Reactivate the site %label?', array('%label' => $site->label));
    $description = t('The site will be able to authenticate and report again using its existing key.');
    $confirm = t('Reactivate');
  }
  else {
    $question = t('Revoke the site %label?', array('%label' => $site->label));
    $description = t('The site can no longer authenticate, but its reported history is kept. You can reactivate it later.');
    $confirm = t('Revoke');
  }

  return confirm_form(
    $form,
    $question,
    'admin/config/remote-site-status-server',
    $description,
    $confirm,
    t('Cancel')
  );
}

/**
 * Submit handler for the revoke/reactivate confirm form.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_site_revoke_form_submit($form,
    &$form_state) {
  $new_status = !empty($form['#reactivate']) ? 'active' : 'revoked';
  db_update('remote_site_status_site')
    ->fields(array('status' => $new_status))
    ->condition('id', $form['#site_id'])
    ->execute();

  $message = ($new_status === 'active')
    ? t('Site reactivated.') : t('Site revoked.');
  backdrop_set_message($message);
  $form_state['redirect'] = 'admin/config/remote-site-status-server';
}

/**
 * Confirm form: permanently delete a site and its reported rows.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 * @param int $site_id
 *   The site id.
 *
 * @return array
 *   The confirm form.
 */
function remote_site_status_server_site_delete_form($form, &$form_state,
    $site_id) {
  $site = remote_site_status_server_site_load_or_404($site_id);
  $form['#site_id'] = $site_id;

  return confirm_form(
    $form,
    t('Are you sure you want to delete the site %label?', array('%label' => $site->label)),
    'admin/config/remote-site-status-server',
    t('This permanently removes the site and the versions it reported. This cannot be undone. To stop a site reporting while keeping its history, revoke it instead.'),
    t('Delete'),
    t('Cancel')
  );
}

/**
 * Submit handler for the site delete confirm form.
 *
 * Removes the site and its site_project rows. Project rows are fleet-wide and
 * left intact (other sites may still reference them).
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_site_delete_form_submit($form,
    &$form_state) {
  $site_id = $form['#site_id'];

  db_delete('remote_site_status_site_project')
    ->condition('site_id', $site_id)
    ->execute();
  db_delete('remote_site_status_site')
    ->condition('id', $site_id)
    ->execute();

  backdrop_set_message(t('Site deleted.'));
  $form_state['redirect'] = 'admin/config/remote-site-status-server';
}

// =============================================================================
// Settings form
// =============================================================================

/**
 * Form builder: module settings.
 *
 * Four operator-tunable values: the security-notes stub toggle (plus the date
 * format used in the stub's {date} token), and the health-threshold defaults
 * (stale cron, quiet site) that the operator can reference in their fleet
 * dashboard Views.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 *
 * @return array
 *   The form definition.
 */
function remote_site_status_server_settings_form($form, &$form_state) {
  $config = config('remote_site_status_server.settings');

  $form['security'] = array(
    '#type' => 'fieldset',
    '#title' => t('Security notes'),
    '#collapsible' => FALSE,
  );

  $form['security']['security_notes_stub'] = array(
    '#type' => 'checkbox',
    '#title' => t('Add &ldquo;{date} - {Version} - &rdquo; stub to notes on new security releases'),
    '#description' => t('When a manual latest-version fetch finds a security release newer than the last reviewed version, automatically prepend a dated stub to the project security notes for the operator to complete.'),
    '#default_value' => $config->get('security_notes_stub'),
  );

  // Build the date-format select from the site's configured formats so the
  // operator can reuse any format they have already set up, and so that
  // editing the format later flows through to new stubs automatically.
  $formats = system_get_date_formats();
  $date_options = array();
  foreach ($formats as $machine_name => $format) {
    if (!empty($format['hidden'])) {
      continue;
    }
    $label = !empty($format['label'])
      ? check_plain($format['label']) : check_plain($machine_name);
    $pattern = !empty($format['pattern'])
      ? ' (' . check_plain($format['pattern']) . ')' : '';
    $date_options[$machine_name] = $label . $pattern;
  }

  $form['security']['security_notes_date_format'] = array(
    '#type' => 'select',
    '#title' => t('Date format for notes stub'),
    '#description' => t('The date format used for the {date} token in the stub line. Choose from the formats configured at <a href="@url">admin/config/regional/date-time</a>.', array('@url' => url('admin/config/regional/date-time'))),
    '#options' => $date_options,
    '#default_value' => $config->get('security_notes_date_format'),
    '#states' => array(
      'visible' => array(
        ':input[name="security_notes_stub"]' => array('checked' => TRUE),
      ),
    ),
  );

  $form['thresholds'] = array(
    '#type' => 'fieldset',
    '#title' => t('Health thresholds'),
    '#description' => t('Number-of-days thresholds used to flag stale cron or quiet sites. Store the values you want to use in your fleet-dashboard Views here so they live in config rather than being hard-coded in each view.'),
    '#collapsible' => FALSE,
  );

  $form['thresholds']['stale_cron_days'] = array(
    '#type' => 'textfield',
    '#title' => t('Stale-cron threshold (days)'),
    '#description' => t('Days since last cron run before a site is considered stale.'),
    '#default_value' => $config->get('stale_cron_days'),
    '#size' => 5,
    '#maxlength' => 4,
    '#required' => TRUE,
    '#element_validate' => array('form_validate_number'),
    '#min' => 1,
  );

  $form['thresholds']['quiet_site_days'] = array(
    '#type' => 'textfield',
    '#title' => t('Quiet-site threshold (days)'),
    '#description' => t('Days since last report before a site is considered to have stopped reporting.'),
    '#default_value' => $config->get('quiet_site_days'),
    '#size' => 5,
    '#maxlength' => 4,
    '#required' => TRUE,
    '#element_validate' => array('form_validate_number'),
    '#min' => 1,
  );

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save settings'),
  );

  return $form;
}

/**
 * Submit handler for the settings form.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_settings_form_submit($form, &$form_state) {
  $config = config('remote_site_status_server.settings');
  $stub = !empty($form_state['values']['security_notes_stub']);
  $date_fmt = $form_state['values']['security_notes_date_format'];
  $config->set('security_notes_stub', $stub);
  $config->set('security_notes_date_format', $date_fmt);
  $config->set('stale_cron_days',
    (int) $form_state['values']['stale_cron_days']);
  $config->set('quiet_site_days',
    (int) $form_state['values']['quiet_site_days']);
  $config->save();

  backdrop_set_message(t('Settings saved.'));
}

// =============================================================================
// Project security review
// =============================================================================

/**
 * Load a project row by id, or redirect to the overview if it does not exist.
 *
 * @param int $project_id
 *   The project id.
 *
 * @return object
 *   The project row. Does not return if missing (redirects instead).
 */
function remote_site_status_server_project_load_or_404($project_id) {
  $project = db_select('remote_site_status_project', 'p')
    ->fields('p')
    ->condition('id', $project_id)
    ->execute()
    ->fetchObject();

  if (!$project) {
    backdrop_set_message(t('Project not found.'), 'error');
    backdrop_goto('admin/config/remote-site-status-server');
  }

  return $project;
}

/**
 * Form builder: security review for a fleet project.
 *
 * Shows the project's current security state and allows the operator to edit
 * the security notes and optionally stamp the project as reviewed — which
 * sets security_reviewed_version to the current latest_security_version and
 * flips security_status to 'Reviewed'. The next fetch will flip it back to
 * 'Needs review' only if a newer security version appears, keeping the
 * reviewed/notes history intact. Accessible at
 * admin/config/remote-site-status-server/projects/{id}/review.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 * @param int $project_id
 *   The project row id.
 *
 * @return array
 *   The form definition.
 */
function remote_site_status_server_project_review_form($form, &$form_state,
    $project_id) {
  $project = remote_site_status_server_project_load_or_404($project_id);
  $form['#project_id'] = $project_id;

  // Read-only project facts for context.
  $form['info'] = array(
    '#type' => 'fieldset',
    '#title' => t('Project'),
    '#collapsible' => FALSE,
  );
  $form['info']['name'] = array(
    '#type' => 'item',
    '#title' => t('Project'),
    '#markup' => check_plain($project->project)
      . ' <em>(' . check_plain($project->type) . ')</em>',
  );

  if (!empty($project->latest_version)) {
    $form['info']['latest_version'] = array(
      '#type' => 'item',
      '#title' => t('Latest version'),
      '#markup' => check_plain($project->latest_version),
    );
  }

  if (!empty($project->latest_security_version)) {
    $form['info']['latest_security_version'] = array(
      '#type' => 'item',
      '#title' => t('Latest security version'),
      '#markup' => check_plain($project->latest_security_version),
    );
  }

  $status_map = array(
    'Needs review' => t('Needs review'),
    'Reviewed' => t('Reviewed'),
  );
  $status_label = !empty($project->security_status)
    ? ($status_map[$project->security_status]
      ?? check_plain($project->security_status))
    : t('—');
  $form['info']['security_status'] = array(
    '#type' => 'item',
    '#title' => t('Security status'),
    '#markup' => $status_label,
  );

  if (!empty($project->security_reviewed_version)) {
    $form['info']['security_reviewed_version'] = array(
      '#type' => 'item',
      '#title' => t('Last reviewed version'),
      '#markup' => check_plain($project->security_reviewed_version),
    );
  }

  // Editable fields.
  $form['security_notes'] = array(
    '#type' => 'textarea',
    '#title' => t('Security notes'),
    '#description' => t('Newest entry first. Each review should record the version assessed and the decision, e.g. <code>12/06/2026 - 1.x-1.2.0 - ok to defer, vuln requires untrusted editor access</code>.'),
    '#default_value' => $project->security_notes ?? '',
    '#rows' => 8,
  );

  // Only offer the "mark as reviewed" action when there is actually a security
  // version to stamp. Checking this box stamps security_reviewed_version and
  // clears the needs_review flag; unchecking it saves notes only.
  if (!empty($project->latest_security_version)) {
    $form['mark_reviewed'] = array(
      '#type' => 'checkbox',
      '#title' => t('Mark as reviewed (stamp %version as the reviewed version)', array('%version' => $project->latest_security_version)),
      '#default_value' => ($project->security_status === 'Needs review')
        ? 1 : 0,
    );
  }

  $form['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
  );

  // Use the destination query parameter when present (e.g. after clicking
  // a review link from a filtered view), but restrict it to internal paths
  // only to prevent open-redirect abuse.
  $cancel_path = 'admin/config/remote-site-status-server';
  if (!empty($_GET['destination'])) {
    $dest = (string) $_GET['destination'];
    // External URLs start with a scheme or protocol-relative //; reject those.
    if (strpos($dest, '://') === FALSE && strpos($dest, '//') !== 0) {
      $cancel_path = $dest;
    }
  }
  $form['cancel'] = array(
    '#type' => 'markup',
    '#markup' => ' ' . l(t('Cancel'), $cancel_path),
  );

  return $form;
}

/**
 * Submit handler for the project security review form.
 *
 * Always saves the edited security notes. If the operator ticked "mark as
 * reviewed", also stamps security_reviewed_version with the current
 * latest_security_version and sets security_status to reviewed.
 *
 * @param array $form
 *   The form array.
 * @param array $form_state
 *   The form state array.
 */
function remote_site_status_server_project_review_form_submit($form,
    &$form_state) {
  $project_id = $form['#project_id'];

  $fields = array(
    'security_notes' => $form_state['values']['security_notes'],
  );

  if (!empty($form_state['values']['mark_reviewed'])) {
    $project = db_select('remote_site_status_project', 'p')
      ->fields('p', array('latest_security_version'))
      ->condition('id', $project_id)
      ->execute()
      ->fetchObject();

    if ($project && !empty($project->latest_security_version)) {
      $fields['security_reviewed_version'] = $project->latest_security_version;
      $fields['security_status'] = 'Reviewed';
    }
  }

  db_update('remote_site_status_project')
    ->fields($fields)
    ->condition('id', $project_id)
    ->execute();

  backdrop_set_message(t('Project security review saved.'));
}
