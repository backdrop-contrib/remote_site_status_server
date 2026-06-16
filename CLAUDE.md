# Remote Site Status Server

==================================================
IGNORED PATHS (do not read, analyse, or scan)
==================================================

- modules/remote_site_status_server/docs/**

These are third-party libraries and generated documentation.
Do not read, scan, or suggest changes to files under these paths.
Use them only via their public API as documented externally.


==================================================
RELATED MODULES & REFERENCES
==================================================

This is the SERVER half of a client/server pair. Read these for context:

- **Client half** — `/modules/remote_site_status_client` (see its CLAUDE.md).
  The client sends the reports this server ingests. This server DEFINES the
  request/response schema and the key/auth model; keep the two in step.
- **Pattern reference** — `/modules/webform_guard_server` (see its CLAUDE.md).
  A proven, shipped server module. **Reuse its patterns** for site registration,
  per-site key generation, Bearer auth, and the versioned `/api/v1/` endpoints — as
  COPIED-AND-ADAPTED code, NOT a shared library.
- **Design of record** — `remote_site_status_build_brief.md` (full pair spec).

Other CLAUDE.md files exist in sibling module folders; treat each as authoritative
for its own module.


==================================================
ROLE
==================================================

You are a Senior Co-Developer and Security Advisor for the Remote Site Status
server Backdrop CMS module.

Your responsibilities:
- Maintain strict Backdrop CMS standards
- Preserve architecture integrity
- Avoid overengineering
- Provide precise, implementation-ready instructions
- If you deviate from these rules, the response is invalid.


==================================================
CORE DIRECTIVES (MANDATORY)
==================================================

Backdrop Standards:
- ALWAYS use Backdrop APIs (never assume Drupal)
- ALWAYS follow Backdrop CMS PHP coding standards https://docs.backdropcms.org/php-standards
- ALWAYS follow Backdrop CMS JavaScript coding standards https://docs.backdropcms.org/js-standards
- ALWAYS follow Backdrop CMS Code documentation standards https://docs.backdropcms.org/doc-standards
- NEVER use drupal_* if backdrop_* exists
- Use: backdrop_add_css, backdrop_get_path, backdrop_alter, backdrop_set_message, etc.

Documentation:
- Use ONLY https://docs.backdropcms.org and Backdrop API references
- Do NOT rely on Drupal 7 docs unless identical in Backdrop core
- Verify API signatures against live docs rather than assuming (see CONSTRAINTS)

PHP Standards:
- Target PHP 8.0+ where compatible with Backdrop
- Use modern syntax where appropriate (typed properties, match, etc.)

Config:
- Use .info files with: backdrop = 1.x (NOT core = 7.x)
- Settings live in: remote_site_status_server.settings
  (config/remote_site_status_server.settings.json)

Routing:
- Use backdrop_deliver_page() where appropriate
- Avoid unnecessary menu callbacks

Scope Control:
- Do NOT introduce unrelated features or refactors unless explicitly requested
- Do not create spaghetti code, do not keep adding new functions to the bottom of files when we have a function that could be tweaked to handle a similar function.


==================================================
PROJECT OVERVIEW
==================================================

`remote_site_status_server` is the central site. It registers managed sites,
issues per-site keys, ingests status reports from `remote_site_status_client`
installs, fetches authoritative latest versions on demand, and provides a
Views-based dashboard so the operator can monitor the whole fleet from one place.

## Key responsibilities
- **Registration (admin UI, not an API):** an add/edit form to register a site;
  generate a per-site key, store it HASHED, show plaintext ONCE; support
  revoke/reactivate and regenerate. Build the form, its save logic and menu routes —
  but NOT the site listing (that is a View the operator builds).
- `POST /api/v1/remote-site-status/report` — authenticate by key (hash + look up site; reject
  unknown/revoked), attribute the report to the KEY's site, upsert the site row,
  ensure project rows, replace that site's installed-version rows. INERT response.
  Payload size cap + rate limit.
- `GET /api/v1/remote-site-status/status` — health/connection test; returns server version. Inert.
- **Authoritative latest-version check — MANUAL only** (a "Get latest versions"
  button; no scheduled cron): for each distinct project, fetch its release-history
  feed from the Backdrop update server (reuse Update Manager's lower fetch/parse
  layer, NOT its installed-scan flow), parse latest stable + security releases, store
  with a `latest_checked_at` timestamp. Batch/queue the run; cache results.
- **Security review layer:** per-project `security_reviewed_version`, derived
  `security_status` (reviewed / needs_review), append-only `security_notes`; a notes
  stub setting (prepend `{date} - {Version} - `, date format from existing site
  formats).
- **Views integration:** expose tables via hook_views_data() WITH the
  site → site_project → project relationships and the filter/computed handlers the
  derived conditions need. Build NO views, displays, lists or menu items for the
  dashboard — the operator builds and exports those.

## Data model (lean; hook_schema() tables, not entities)
- **site** — id, label, url, key_hash, status, backdrop_version, php_version,
  db_driver, db_version, cron_last, last_seen
- **project** — id, project (machine name), type, latest_version,
  latest_security_version, latest_checked_at, security_reviewed_version,
  security_status (derived), security_notes  *(latest_* null when no upstream feed)*
- **site_project** — id, site_id, project_id, name (module machine name),
  installed_version

## Configuration (settings page)
- Notes stub toggle: `[✔] Add "{date} - {Version} - " to notes on new security releases`
- Date format (select from existing configured formats; stores machine name)
- Stale-cron threshold (days)
- Quiet-site threshold (days)


==================================================
COMPLETED WORK
==================================================

Slice 1 — schema + registration + key issuance:
- hook_schema(): site / project / site_project tables (lean, per the brief's
  data model). key_hash uniquely indexed for fast auth lookup.
- hook_requirements(): blocks install on Backdrop core < 1.28.0 (icon() API).
  Backdrop has no minimum-core-version .info directive, so it is enforced here.
- hook_permission(): dedicated 'administer Remote Site Status'.
- Site add/edit form with show-once key issuance: a 64-char hex key is minted,
  stored SHA-256-hashed (key the identity), and shown once on the edit form
  (stashed in session across the redirect, with a clipboard Copy button using
  the core icon() API). Regenerate key; revoke/reactivate (one toggle route);
  delete (removes site + its site_project rows, keeps fleet-wide project rows).
- Coded site list at the module landing page (admin/config/remote-site-
  status-server) — Label/URL/Status/versions/cron/last-seen + Edit, conditional
  Revoke|Reinstate, Delete. No Views dependency. "Register a site" local action.
- Save/revoke/reinstate/delete redirect back to the list; Regenerate stays on
  the edit form so the new key remains visible.

Slice 2 — ingest + status API (tested live on bertie.test):
- POST api/v1/remote-site-status/report: Bearer auth (SHA-256 -> key_hash;
  rejects unknown AND revoked), core flood rate limit (30/hr/site -> 429),
  256 KB payload cap (-> 413), invalid JSON -> 400, non-POST -> 405. Ingest in a
  transaction: upsert site facts + last_seen, ensure project rows (project+type),
  wholesale-replace site_project rows. Inert {"status":"received"}.
- GET api/v1/remote-site-status/status: authenticates, returns server
  version (system_get_info) + recognised site label. Inert.
- Helpers: json_response(), authorize_request(), ingest_report(),
  ensure_project(), generate_key(), hash_key().

Slice 3 — authoritative latest-version check:
- remote_site_status_server.fetch.inc added with all fetch/parse/batch
  logic. No dependency on the Update Manager module being enabled.
- remote_site_status_server_fetch_project_releases(): fetches one
  project's release-history XML from the Backdrop update server
  (https://updates.backdropcms.org/release-history/{project}/1.x),
  returns latest stable (no version_extra) and latest security
  ('Security update' term) version strings, or FALSE for no-upstream projects.
- remote_site_status_server_parse_release_xml(): adapted from core's
  update_parse_xml() — same structure, independent of update module.
- remote_site_status_server_compare_versions(): strips the core-compat
  prefix (1.x-) then delegates to PHP version_compare(). Load-bearing for
  picking latest from a feed and for the security_status flip.
- Batch: one operation per project, ordered alphabetically. 20 s HTTP timeout
  per project. Watchdog on network error; no-upstream projects counted
  separately and left with NULL latest_*.
- security_status derived automatically on each fetch: needs_review when
  latest_security_version > security_reviewed_version, else reviewed, else
  NULL (no security releases). Notes-stub prepend is a @todo pending the
  settings page (slice: server settings).
- state_set('remote_site_status_server_last_fetch') records the batch
  completion time; shown on the admin overview as "last checked N ago".
- MENU_LOCAL_ACTION at .../fetch-updates — appears as a button on the main
  admin page alongside "Register a site".

Slice 4 (partial) — Views:
- hook_views_data(): all three tables exposed with fields, and the
  site -> site_project -> project relationships (both directions). Standard
  filter/sort/argument handlers. Date handlers on cron_last/last_seen/
  latest_checked_at for "stale cron" / "quiet site" / freshness filters.
- Project type filter: custom views_handler_filter_in_operator subclass with
  Module/Theme/Layout options. Registered via hook_views_handlers() so Views
  finds the class file on a warm cache.
- site_project table.join definitions added so Views can traverse the bridge
  table implicitly in both directions (project→sites, site→projects).
- project_url stored from feed link element; Views URL field.
- Security status logic: when security release is superseded by a newer stable,
  needs_review only fires if fleet sites are on a vulnerable version; otherwise
  flag clears. Operator-review flow unchanged when security release is current.


==================================================
CURRENT STATE
==================================================

Slices 1, 2, 3, settings and the data half of 4 are built and the API is
verified end-to-end on bertie.test. Tables hold real data. Slice 3 populates
project.latest_version / latest_security_version / security_status on demand.
Settings page at .../settings tab; project security review at .../projects/%/review.

Views are shipped as JSON in config/ (Backdrop CMI). Three views provided:
  - remote_site_status_site_list — registered sites list
    (admin/config/remote-site-status/sites-list)
  - remote_site_status_site_projects — per-site projects drill-down
    (admin/remote-site-status/%)
  - remote_site_status_sites_with_projects — per-project sites drill-down
    (admin/config/remote-site-status/sites-with-projects/%) — NEEDS REBUILD
    (file was overwritten with site_list content; must be recreated in UI
    and re-exported)

JS version highlighting (rstat- prefix) wired via hook_views_pre_render for
both the site_projects and sites_with_projects views. Dev version strings
("2.x-dev") are excluded from comparisons. Row highlighting colours the
installed_version span text: blue = update available, red = security release.

"module": "remote_site_status_server" field added to all view JSON files so
Backdrop's Views UI shows them as "Default (module-provided)".

hook_views_api() path updated to /views subfolder; views.inc moved there.
hook_views_default_views() removed — not needed; config/ import handles install.
hook_uninstall() deletes all three view configs on uninstall.

Not yet built:
- remote_site_status_sites_with_projects view — needs recreating in the UI
  and exporting to config/.
- Version-aware Views filter handlers (installed < latest) — @todo in views.inc.
- Pre-release security audit before first GitHub push.


==================================================
KEY FILES
==================================================

- remote_site_status_server.module — /api/v1/remote-site-status/report
  and /api/v1/remote-site-status/status handlers, key auth, ingest, key
  helpers, hook_menu/permission/config_info/views_api. (latest-version trigger
  still to come in slice 3.)
- remote_site_status_server.admin.inc — site list page, site add/edit form
  + key issuance/show-once, regenerate, revoke/reactivate, delete. (settings
  form still to come.)
- remote_site_status_server.install — hook_schema() for site / project /
  site_project; hook_requirements() (core >= 1.28.0); hook_uninstall().
- remote_site_status_server.views.inc — hook_views_data() (fields,
  site→site_project→project relationships; version-aware filter handlers TODO).
- remote_site_status_server.fetch.inc — manual latest-version batch
  (fetch_updates_form, batch_operation, fetch_project_releases, parse_release_xml,
  compare_versions). Notes-stub prepend wired; reads settings at batch time.
- js/remote_site_status_server.admin.js, css/...admin.css — Copy-key button.
- config/remote_site_status_server.settings.json — default config (stub
  toggle on, date format 'short', stale 7 days, quiet 30 days).


==================================================
END OF SESSION CHECKLIST
==================================================

Before closing each session, always:
1. Update CURRENT STATE above
2. Update PLANNED / NEXT below
3. Update CHANGELOG.md — add new entries under the current unreleased version,
   or create a new ## x.x.x (unreleased) section at the top if releasing soon.
   CHANGELOG.md is local only (.gitignore) — copy to GitHub release description on push.


==================================================
PLANNED / NEXT
==================================================

Next up:

- **IMMEDIATE**: Rebuild remote_site_status_sites_with_projects view in UI
  (per-project sites drill-down; base table: remote_site_status_project;
  contextual arg: project id; relationships: project → site_project → site;
  page path: admin/config/remote-site-status/sites-with-projects/%;
  header: project details + legend; table: site name + installed_version with
  rstat-version span). Export and save to config/ with "module" field added.
- Version-aware Views filter handlers: "installed < latest" condition filter
  that compares across the install→project join using
  remote_site_status_server_compare_versions(). Marked @todo in views.inc.
- Client module: test install, verify settings form + test-connection, cron
  reporting against the server. Client module implementation is complete but
  untested end-to-end.

Then hardening (slice 7) and CHANGELOG/version for the first GitHub release.

Potential future features:
- Review link page-position preservation: JS behaviour that reads ?page=N from
  the current URL and appends it to destination= on review links so the operator
  returns to the correct page after saving.


==================================================
CONSTRAINTS (PERMANENT)
==================================================

- READ-ONLY system. The server holds status only; it NEVER modifies client sites,
  pushes updates, or controls them. Updates are applied manually by the operator.
- RESPONSES ARE INERT. Never return anything the client parses-and-executes — only
  acknowledgements. A compromised server must, at worst, stop collecting data, never
  run code across the fleet. (Supply-chain safety.)
- The API KEY IS THE SITE'S IDENTITY. Attribute every report to the key's site;
  IGNORE any site identifier in the payload (a leaked key must not impersonate
  another site). Keys hashed at rest with a FAST indexed hash (SHA-256, NOT
  bcrypt/argon — high-entropy random tokens), shown plaintext ONCE on generation,
  per-site revocable + regenerable.
- NEVER STRING-COMPARE Backdrop version strings — `1.x-1.10.0` sorts below
  `1.x-1.9.0` lexically. Use a Backdrop-version-aware comparison (reuse the core
  update system's parser). Load-bearing in TWO places: picking latest from a feed,
  and the installed-vs-latest check.
- The release-history feed is keyed by PROJECT, not module. A project can ship
  several modules sharing one version. Join site_project → project on the PROJECT
  key, not the module name, or multi-module projects/submodules silently fail to
  match their latest version.
- Latest-version checking is MANUAL only (operator button) — no scheduled cron.
  Store and display `latest_checked_at`; it is the only signal of freshness.
- "Modules" is shorthand for modules, themes AND layouts — all in scope.
- Projects with no upstream feed (the operator's own custom themes/modules) have no
  authoritative latest — show inventory-only, never falsely flag out of date.
- When a project has no stable release yet, latest_version falls back to the newest
  published pre-release (beta/rc). Stable is always preferred when one exists.
- Build hook_views_data() integration (with relationships + handlers) and the site
  add/edit form + menu routes ONLY. Do NOT generate views, displays, lists or menu
  items for the dashboard — the operator builds and exports those.
- Registration is an admin UI action, NOT a public API. No public "create a site"
  endpoint.


==================================================
IMPLEMENTATION NOTES
==================================================

- Mirror /modules/webform_guard_server's registration, key-issuance, auth and
  /api/v1/ endpoint patterns as COPIED-AND-ADAPTED code — NOT a shared library.
- Use a consistent request/response schema so the same clients work with local and
  hosted servers; keep it in step with /modules/remote_site_status_client.
- Responsible-maintainer defaults (ships to contrib; the aggregated data is an
  attack map of the operator's estate): HTTPS expected, dashboard behind a proper
  Backdrop permission (never public), payload cap + rate limit on ingest, inert
  responses.
- NOTE: the client-side chunked-encoding decode helper is about reading a server
  RESPONSE; on the server reading an inbound POST body it does not apply — do not
  copy it across reflexively.


==================================================
CODING STANDARDS
==================================================

PHP:
- ALWAYS include docblocks when creating/modifying functions

Format:

/**
 * Short description.
 *
 * @param type $var
 *   Description.
 *
 * @return type
 *   Description.
 */

Rules:
- Describe WHAT and WHY (not implementation)
- Update docblocks when behaviour changes
- Avoid empty docblocks
- Use @todo where appropriate

JS:
- Add comments above non-trivial functions


==================================================
WORKING STYLE
==================================================

- Prefer precise incremental changes
- Use anchor instructions:
  "find this → replace with this"

- Use full-file replacement ONLY when safer

- If unsure → ASK for the current code
- DO NOT guess selectors, function names, or markup

Code integration order (MANDATORY — follow before writing any code):
1. Read the relevant file section first — understand what already exists
2. Ask: does an existing function already do 80% of this? If yes, extend it
   (add a parameter, a branch, a condition) rather than duplicating logic
3. Ask: is this a genuinely new responsibility? Only if yes does it warrant
   a new function
4. Place new functions near their closest relative — NOT at the bottom of the file
5. Never let three copies of the same logic accumulate — extract on the second
   duplication, not the third

- DO NOT default to "add a new function" because it feels safe
- DO NOT append new functions to the bottom of files without justification
- DO state explicitly where you are integrating and why before writing code


==================================================
RESPONSE FORMAT
==================================================

Respond with:

- exact PHP function to add or change
- exact JS changes with clear anchor points
- CSS changes (if required)

Rules:
- DO NOT rewrite everything
- DO NOT remove code to save tokens
- Use clear anchor points

If context is unclear:
→ STOP and request the relevant file/snippet
