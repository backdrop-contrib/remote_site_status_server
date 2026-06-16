<?php

/**
 * @file
 * Views integration for Remote Site Status Server.
 *
 * Exposes the three status tables, their fields, and — critically — the
 * relationships between them (site -> site_project -> project) so the operator
 * can build the dashboard as a View. This module exposes the data only; it
 * builds no views, displays, lists or menu items.
 *
 * The two version-aware "out of date" conditions (installed_version <
 * latest_version, and the unreviewed-security condition) require a custom
 * filter handler that compares Backdrop version strings correctly rather than
 * lexically. That handler is added with the latest-version check slice (it
 * shares the version comparator); see the @todo at the foot of this file.
 */

/**
 * Views filter handler: needs-security-review boolean toggle.
 *
 * Presents a Yes/No widget in the Views UI but queries the security_status
 * column for 'Needs review'. No extra DB column required.
 */
class remote_site_status_server_handler_filter_needs_review
  extends views_handler_filter {

  /**
   * Render the filter widget as a three-option select: Any / Yes / No.
   *
   * Uses non-empty string keys ('yes'/'no') so Views' filter_value_empty()
   * check does not treat the "No" selection as an empty/skipped value the
   * way it would with '0'.
   */
  function value_form(&$form, &$form_state) {
    $form['value'] = array(
      '#type' => 'select',
      '#title' => t('Value'),
      '#options' => array(
        'yes' => t('Yes'),
        'no'  => t('No'),
      ),
      '#default_value' => isset($this->value) ? $this->value : 'yes',
    );
  }

  /**
   * Apply the filter condition, or skip entirely for "Any".
   *
   * Views may wrap the submitted value in an array; unwrap before comparing.
   */
  function query() {
    $val = is_array($this->value) ? reset($this->value) : $this->value;
    if ($val === '' || $val === NULL) {
      return;
    }
    $this->ensure_my_table();
    $field = "$this->table_alias.$this->real_field";
    if ($val === 'yes') {
      $this->query->add_where(
        $this->options['group'], $field, 'Needs review', '='
      );
    }
    else {
      $this->query->add_where_expression($this->options['group'],
        "$field != 'Needs review' OR $field IS NULL");
    }
  }

}

/**
 * Views filter handler: project type select list.
 *
 * Provides a checkboxes/select filter for the fixed set of project types
 * (module, theme, layout) rather than a free-text field.
 */
class remote_site_status_server_handler_filter_project_type
  extends views_handler_filter_in_operator {

  /**
   * Populate the value options for the project type filter.
   */
  function get_value_options() {
    $this->value_options = array(
      'module' => t('Module'),
      'theme' => t('Theme'),
      'layout' => t('Layout'),
    );
  }

}

/**
 * Implements hook_views_data().
 */
function remote_site_status_server_views_data() {
  $data = array();

  // ---------------------------------------------------------------------------
  // site
  // ---------------------------------------------------------------------------
  $data['remote_site_status_site']['table']['group'] = t('Remote Site Status: site');
  $data['remote_site_status_site']['table']['base'] = array(
    'field' => 'id',
    'title' => t('Remote Site Status site'),
    'help' => t('A registered client site reporting its status to this server.'),
  );

  $data['remote_site_status_site']['id'] = array(
    'title' => t('Site ID'),
    'help' => t('The unique internal ID of the site.'),
    'field' => array('handler' => 'views_handler_field_numeric'),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_numeric'),
    'argument' => array('handler' => 'views_handler_argument_numeric'),
  );

  $data['remote_site_status_site']['label'] = array(
    'title' => t('Label'),
    'help' => t('The operator-facing name / grouping label for the site.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['url'] = array(
    'title' => t('URL'),
    'help' => t('The site URL.'),
    'field' => array('handler' => 'views_handler_field_url', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['status'] = array(
    'title' => t('Status'),
    'help' => t('Lifecycle state: active or revoked.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['backdrop_version'] = array(
    'title' => t('Backdrop version'),
    'help' => t('Backdrop core version last reported by the site.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['php_version'] = array(
    'title' => t('PHP version'),
    'help' => t('PHP version last reported by the site. Filter as a string to find sites below a threshold.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['db_driver'] = array(
    'title' => t('DB driver'),
    'help' => t('Database driver last reported (e.g. mysql, mariadb).'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['db_version'] = array(
    'title' => t('DB version'),
    'help' => t('Database server version string last reported.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site']['cron_last'] = array(
    'title' => t('Last cron run'),
    'help' => t('When the site last ran cron, as reported. Use a date filter (offset from now) to flag sites whose cron has not run in N days.'),
    'field' => array('handler' => 'views_handler_field_date', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort_date'),
    'filter' => array('handler' => 'views_handler_filter_date'),
  );

  $data['remote_site_status_site']['last_seen'] = array(
    'title' => t('Last seen'),
    'help' => t('When this site last reported. Use a date filter to find sites that have stopped reporting.'),
    'field' => array('handler' => 'views_handler_field_date', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort_date'),
    'filter' => array('handler' => 'views_handler_filter_date'),
  );

  // Reverse relationship: site -> the installed items it reports.
  $data['remote_site_status_site']['site_project'] = array(
    'title' => t('Installed items'),
    'help' => t('The modules/themes/layouts installed on this site.'),
    'relationship' => array(
      'base' => 'remote_site_status_site_project',
      'base field' => 'site_id',
      'relationship field' => 'id',
      'handler' => 'views_handler_relationship',
      'label' => t('Installed items'),
    ),
  );

  // ---------------------------------------------------------------------------
  // project
  // ---------------------------------------------------------------------------
  $data['remote_site_status_project']['table']['group'] = t('Remote Site Status: project');
  $data['remote_site_status_project']['table']['base'] = array(
    'field' => 'id',
    'title' => t('Remote Site Status project'),
    'help' => t('A distinct project seen across the fleet, with its authoritative latest/security versions.'),
  );

  $data['remote_site_status_project']['id'] = array(
    'title' => t('Project ID'),
    'help' => t('The unique internal ID of the project.'),
    'field' => array('handler' => 'views_handler_field_numeric'),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_numeric'),
    'argument' => array('handler' => 'views_handler_argument_numeric'),
  );

  $data['remote_site_status_project']['project'] = array(
    'title' => t('Project machine name'),
    'help' => t('The .info project machine name. Filter here for "who runs project X".'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['type'] = array(
    'title' => t('Type'),
    'help' => t('Project type: module, theme or layout.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array(
      'handler' => 'remote_site_status_server_handler_filter_project_type',
    ),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['latest_version'] = array(
    'title' => t('Latest version'),
    'help' => t('Latest stable release from the update server. Empty for custom projects with no upstream feed (filter "is empty" for inventory-only projects).'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['latest_security_version'] = array(
    'title' => t('Latest security version'),
    'help' => t('Latest release flagged as a security release. Empty when none / no upstream feed.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['latest_checked_at'] = array(
    'title' => t('Latest checked at'),
    'help' => t('When the latest-version feed was last fetched for this project. The only freshness signal under manual checking.'),
    'field' => array('handler' => 'views_handler_field_date', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort_date'),
    'filter' => array('handler' => 'views_handler_filter_date'),
  );

  $data['remote_site_status_project']['project_url'] = array(
    'title' => t('Project URL'),
    'help' => t('Project page URL from the Backdrop release-history feed. Empty for custom projects with no upstream feed.'),
    'field' => array('handler' => 'views_handler_field_url', 'click sortable' => FALSE),
    'filter' => array('handler' => 'views_handler_filter_string'),
  );

  $data['remote_site_status_project']['security_reviewed_version'] = array(
    'title' => t('Security reviewed version'),
    'help' => t('The security version the operator last assessed.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['security_status'] = array(
    'title' => t('Security review status'),
    'help' => t('Derived review state: reviewed or needs_review. Filter to needs_review for the priority list.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_project']['security_needs_review'] = array(
    'title' => t('Needs security review'),
    'help' => t('Boolean filter: Yes = needs_review, No = reviewed or no security releases.'),
    'real field' => 'security_status',
    'filter' => array(
      'handler' => 'remote_site_status_server_handler_filter_needs_review',
    ),
  );

  $data['remote_site_status_project']['security_notes'] = array(
    'title' => t('Security notes'),
    'help' => t('Append-only, newest-first log of operator judgements about this project.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => FALSE),
    'filter' => array('handler' => 'views_handler_filter_string'),
  );

  // Reverse relationship: project -> the site installs that reference it.
  $data['remote_site_status_project']['site_project'] = array(
    'title' => t('Installs'),
    'help' => t('The per-site installs of this project across the fleet.'),
    'relationship' => array(
      'base' => 'remote_site_status_site_project',
      'base field' => 'project_id',
      'relationship field' => 'id',
      'handler' => 'views_handler_relationship',
      'label' => t('Installs'),
    ),
  );

  // ---------------------------------------------------------------------------
  // site_project (the join; the natural base for the dashboard)
  // ---------------------------------------------------------------------------
  $data['remote_site_status_site_project']['table']['group'] = t('Remote Site Status: install');
  $data['remote_site_status_site_project']['table']['base'] = array(
    'field' => 'id',
    'title' => t('Remote Site Status installed'),
    'help' => t('A specific module/theme/layout installed on a site, at a version. The natural base for the fleet dashboard.'),
  );

  // Implicit join definitions so Views can traverse the bridge table in both
  // directions without the operator needing to manually chain relationships.
  // project → site_project: "which sites have this project?"
  // site → site_project: "which projects does this site have?"
  $data['remote_site_status_site_project']['table']['join'] = array(
    'remote_site_status_project' => array(
      'left_field' => 'id',
      'field' => 'project_id',
    ),
    'remote_site_status_site' => array(
      'left_field' => 'id',
      'field' => 'site_id',
    ),
  );

  $data['remote_site_status_site_project']['id'] = array(
    'title' => t('Install ID'),
    'help' => t('The unique internal ID of the install row.'),
    'field' => array('handler' => 'views_handler_field_numeric'),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_numeric'),
    'argument' => array('handler' => 'views_handler_argument_numeric'),
  );

  $data['remote_site_status_site_project']['name'] = array(
    'title' => t('Item machine name'),
    'help' => t('The specific module/theme/layout machine name (a project may ship several).'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  $data['remote_site_status_site_project']['installed_version'] = array(
    'title' => t('Installed version'),
    'help' => t('The version of this item installed on this site, as reported.'),
    'field' => array('handler' => 'views_handler_field', 'click sortable' => TRUE),
    'sort' => array('handler' => 'views_handler_sort'),
    'filter' => array('handler' => 'views_handler_filter_string'),
    'argument' => array('handler' => 'views_handler_argument_string'),
  );

  // Relationship: install -> its site.
  $data['remote_site_status_site_project']['site_id'] = array(
    'title' => t('Site ID'),
    'help' => t('The site this item is installed on.'),
    'field' => array('handler' => 'views_handler_field_numeric'),
    'filter' => array('handler' => 'views_handler_filter_numeric'),
    'argument' => array('handler' => 'views_handler_argument_numeric'),
    'relationship' => array(
      'base' => 'remote_site_status_site',
      'base field' => 'id',
      'handler' => 'views_handler_relationship',
      'label' => t('Site'),
    ),
  );

  // Relationship: install -> its project (the version-carrying inventory row).
  $data['remote_site_status_site_project']['project_id'] = array(
    'title' => t('Project'),
    'help' => t('The project this item belongs to (carries the authoritative latest version).'),
    'field' => array('handler' => 'views_handler_field_numeric'),
    'filter' => array('handler' => 'views_handler_filter_numeric'),
    'argument' => array('handler' => 'views_handler_argument_numeric'),
    'relationship' => array(
      'base' => 'remote_site_status_project',
      'base field' => 'id',
      'handler' => 'views_handler_relationship',
      'label' => t('Project'),
    ),
  );

  // @todo Add custom version-aware filter handlers (built with the
  //   latest-version check slice, which provides the Backdrop version
  //   comparator): "update available" (installed_version < latest_version) and
  //   "unreviewed security update" (installed_version < latest_security_version
  //   AND security_status = needs_review). These compare across the
  //   install->project relationship and must not string-compare versions.

  return $data;
}

