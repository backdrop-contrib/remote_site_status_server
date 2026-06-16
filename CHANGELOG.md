# Changelog — Remote Site Status Server

All notable changes to this module are recorded here. CHANGELOG.md is local
only (gitignored); copy the relevant section into the GitHub release description
on push.

## 1.x-0.1.0 (unreleased)

Initial build. Server half of the Remote Site Status client/server pair.

### Added

- **Schema** (`hook_schema`): `remote_site_status_site`,
  `remote_site_status_project`, `remote_site_status_site_project`.
  `key_hash` uniquely indexed for fast authentication lookup.
- **Install requirement** (`hook_requirements`): blocks installation on Backdrop
  core older than 1.28.0 (required for the `icon()` API).
- **Permission**: dedicated `administer Remote Site Status`.
- **Site registration UI**: add/edit form with show-once API key issuance — a
  64-character hex key minted, stored SHA-256-hashed (the key is the site's
  identity), shown once on the edit form with a clipboard Copy button. Regenerate
  key; revoke/reactivate (single toggle route); delete.
- **Site list**: the module landing page lists registered sites with status,
  reported versions, cron/last-seen, and Edit / conditional Revoke|Reinstate /
  Delete actions. No Views dependency.
- **API — `POST api/v1/remote-site-status/report`**: Bearer-key auth
  (hashed lookup; rejects unknown and revoked sites), per-site rate limit
  (30/hour via core flood control), 256 KB payload cap, JSON validation. Ingests
  in a transaction: upserts site facts and `last_seen`, ensures a project row per
  reported project (keyed by project + type), and wholesale-replaces the site's
  installed-version rows. Inert `{"status":"received"}` response.
- **API — `GET api/v1/remote-site-status/status`**: authenticated health /
  connection test returning the server version and recognised site label. Inert.
- **Views integration** (`hook_views_data`): all three tables exposed with
  fields and the site → site_project → project relationships (both directions),
  plus date handlers for stale-cron / quiet-site / freshness filtering. (Views,
  displays and menu items are left to the operator to build and export.)

- **Settings page** (`admin/config/remote-site-status-server/settings`, MENU_LOCAL_TASK tab):
  - Security-notes stub toggle: when on, a `{date} - {Version} - ` line is prepended to
    `security_notes` automatically whenever a latest-version fetch flips a project to
    `needs_review`. Date format is a select sourced from the site's configured date formats
    (machine name stored; edits to the format flow through to new stubs).
  - Stale-cron threshold (days, default 7) and quiet-site threshold (days, default 30):
    stored in CMI for reference in fleet-dashboard Views.
  - Default config in `config/remote_site_status_server.settings.json`.
- **Project security review form** (`admin/config/remote-site-status-server/projects/{id}/review`):
  displays the project's security state (latest and reviewed versions, current status),
  provides an editable `security_notes` textarea, and optionally stamps
  `security_reviewed_version = latest_security_version` + `security_status = reviewed`
  when the operator ticks "Mark as reviewed". The next fetch re-flips to `needs_review`
  only if a newer advisory appears, keeping notes intact.
- **Notes stub wired** in `fetch_batch_operation()`: reads the stub toggle and date format
  from config, prepends the stub line on `needs_review` flip. No-op when toggle is off.

- **Authoritative latest-version check** (`remote_site_status_server.fetch.inc`):
  manual "Fetch latest versions" local action button on the admin page triggers a
  batched fetch of the Backdrop release-history feed for every project in the fleet
  inventory. One HTTP request per project (20 s timeout); no dependency on the Update
  Manager module being enabled.
  - Parses latest stable release (no pre-release suffix) and latest security release
    (`Security update` term) from the feed XML. Falls back to the latest pre-release
    (beta, rc, etc.) when no stable release exists yet, so projects like `entityform`
    show a meaningful version rather than NULL.
  - Projects with no upstream feed (404/501 or custom modules) leave `latest_*` as
    NULL and are counted as inventory-only — never falsely flagged out of date.
  - `security_status` is derived automatically on each fetch: `needs_review` when
    `latest_security_version` is newer than the operator's `security_reviewed_version`;
    `reviewed` otherwise; NULL when no security releases exist.
  - `latest_checked_at` stored per project row; global last-fetch time stored in
    state and displayed on the admin overview as "Project versions last checked N ago".
  - Notes-stub prepend (`@todo`) wired pending the settings page.
- **Version comparator** (`remote_site_status_server_compare_versions`): strips
  the Backdrop core-compat prefix (`1.x-`) then uses PHP `version_compare()`,
  correctly ordering `1.x-1.10.0` above `1.x-1.9.0` where string comparison would not.

- **project_url field**: populated from the `link` element in the Backdrop
  release-history feed during "Fetch latest versions". Points to the
  backdropcms.org project page (which itself links to GitHub). NULL for custom
  projects with no upstream feed. Exposed in Views as a clickable URL field.
- **Smarter security_status derivation**: when the latest security release has
  been superseded by a newer stable release, `needs_review` is only set if
  fleet sites are actually running a version older than the security release.
  If all sites are on a safe version the flag is cleared (NULL). When the
  security release is at or beyond the latest stable the existing
  operator-review flow applies unchanged.
- **Project type filter** in Views: custom `views_handler_filter_in_operator` subclass
  (`remote_site_status_server_handler_filter_project_type`) provides a
  checkboxes/select filter for Module / Theme / Layout rather than a free-text field.
  Registered via `hook_views_handlers()` so Views can locate the class file on a
  warm cache without re-including views.inc.
- **site_project join definitions**: `table.join` entries added to
  `remote_site_status_site_project` so Views can traverse the bridge table in
  both directions (project → sites, site → projects) without the operator needing
  to chain relationships manually in the UI.

- **Project security review UX**: removed the post-save redirect so the form
  reloads in place after saving. Added a Cancel link that respects the
  `?destination=` query parameter, allowing Views links to return the operator
  to their view (including page number when manually appended). Falls back to
  the admin overview when no destination is set.

- **Views shipped with module** (config/ JSON, Backdrop CMI):
  - `remote_site_status_site_list` — registered sites list with Operations
    dropdown (Edit / Revoke / Delete).
  - `remote_site_status_site_projects` — per-site drill-down: site details
    header, 61-item paginated project list, Type + security + installed-version
    columns, exposed Type and project-name filters.
  - `remote_site_status_sites_with_projects` — per-project sites drill-down
    (needs rebuild; see known issues).
  - `"module": "remote_site_status_server"` field in all view JSON files so
    Views UI shows them as "Default (module-provided)".
  - `hook_uninstall()` deletes all three view configs on uninstall.
- **JS version highlighting** (`js/remote_site_status_server.admin.js`,
  `css/remote_site_status_server.admin.css`):
  - `rstat-` CSS/JS prefix (renamed from ambiguous `rss-` prefix).
  - Installed version span coloured blue (update available) or red (security
    release) via data attributes (`data-installed`, `data-latest`,
    `data-security`) on the `rstat-version` span in the view field rewrite.
  - Dev version strings (e.g. `2.x-dev`) excluded from comparisons.
  - Loaded via `hook_views_pre_render` on both `remote_site_status_site_projects`
    and `remote_site_status_sites_with_projects` views.
  - `rstat-project-version` span on the project version field colours it red
    when the latest version is itself a security release.
- **Views integration moved** to `views/` subfolder; `hook_views_api()` path
  updated accordingly. `hook_views_default_views()` removed (not needed in
  Backdrop CMI approach).

### Notes

- API responses are deliberately inert — acknowledgements only, never anything a
  client parses and executes (supply-chain safety).
- Verified end-to-end on bertie.test: status auth, report ingest, and the
  wholesale-replace ingest rule all confirmed against the database.
