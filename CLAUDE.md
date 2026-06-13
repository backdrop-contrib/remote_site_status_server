# Multisite Status Server (Acuity)

==================================================
IGNORED PATHS (do not read, analyse, or scan)
==================================================

- modules/acuity_multisite_status_server/docs/**

These are third-party libraries and generated documentation.
Do not read, scan, or suggest changes to files under these paths.
Use them only via their public API as documented externally.


==================================================
RELATED MODULES & REFERENCES
==================================================

This is the SERVER half of a client/server pair. Read these for context:

- **Client half** — `/modules/acuity_multisite_status_client` (see its CLAUDE.md).
  The client sends the reports this server ingests. This server DEFINES the
  request/response schema and the key/auth model; keep the two in step.
- **Pattern reference** — `/modules/webform_guard_server` (see its CLAUDE.md).
  A proven, shipped server module. **Reuse its patterns** for site registration,
  per-site key generation, Bearer auth, and the versioned `/api/v1/` endpoints — as
  COPIED-AND-ADAPTED code, NOT a shared library.
- **Design of record** — `acuity_multisite_status_build_brief.md` (full pair spec).

Other CLAUDE.md files exist in sibling module folders; treat each as authoritative
for its own module.


==================================================
ROLE
==================================================

You are a Senior Co-Developer and Security Advisor for the Acuity Multisite Status
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
- Settings live in: acuity_multisite_status_server.settings
  (config/acuity_multisite_status_server.settings.json)

Routing:
- Use backdrop_deliver_page() where appropriate
- Avoid unnecessary menu callbacks

Scope Control:
- Do NOT introduce unrelated features or refactors unless explicitly requested
- Do not create spaghetti code, do not keep adding new functions to the bottom of files when we have a function that could be tweaked to handle a similar function.


==================================================
PROJECT OVERVIEW
==================================================

`acuity_multisite_status_server` is the central site. It registers managed sites,
issues per-site keys, ingests status reports from `acuity_multisite_status_client`
installs, fetches authoritative latest versions on demand, and provides a
Views-based dashboard so the operator can monitor the whole fleet from one place.
Standalone Acuity-family utility (include the standard Acuity disclaimer in the
README).

## Key responsibilities
- **Registration (admin UI, not an API):** an add/edit form to register a site;
  generate a per-site key, store it HASHED, show plaintext ONCE; support
  revoke/reactivate and regenerate. Build the form, its save logic and menu routes —
  but NOT the site listing (that is a View the operator builds).
- `POST /api/v1/report` — authenticate by key (hash + look up site; reject
  unknown/revoked), attribute the report to the KEY's site, upsert the site row,
  ensure project rows, replace that site's installed-version rows. INERT response.
  Payload size cap + rate limit.
- `GET /api/v1/status` — health/connection test; returns server version. Inert.
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

- None yet. Pre-development.


==================================================
CURRENT STATE
==================================================

Pre-development. Design finalised in acuity_multisite_status_build_brief.md.
No code written, no repo created yet. Client half tracked separately in
/modules/acuity_multisite_status_client.


==================================================
KEY FILES (planned)
==================================================

- acuity_multisite_status_server.module — /api/v1/report and /api/v1/status
  handlers, key auth, ingest, latest-version fetch trigger
- acuity_multisite_status_server.admin.inc — site add/edit form, settings form,
  dashboard helpers
- acuity_multisite_status_server.install — hook_schema() for site / project /
  site_project; hook_uninstall()
- acuity_multisite_status_server.views.inc — hook_views_data() (fields,
  site→site_project→project relationships, filter/computed handlers)


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

See the build order in acuity_multisite_status_build_brief.md. Suggested first
slice: the site / project / site_project schema + the site add/edit form + key
issuance (build-order steps 1–2), since everything hangs off those tables. Confirm
structure and the registration/key storage approach, and ask questions, before
writing code. Discuss with user before starting.


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
  hosted servers; keep it in step with /modules/acuity_multisite_status_client.
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
