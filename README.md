# Remote Site Status Server
**Remote Site Status Server** — A centralised monitoring hub for tracking the technical health and module versions of multiple Backdrop CMS sites from a single admin interface.

Remote Site Status Server is a Backdrop CMS module that acts as the intelligence hub for the Remote Site Status system. It receives status reports from remote client sites running the companion `remote_site_status_client` module, stores site facts and installed project versions (modules, themes, and layouts), and fetches authoritative latest-version data from the Backdrop update server on demand. A single Remote Site Status Server installation can monitor an unlimited number of client sites, each registered with its own API key. Run your own server without restrictions, even on the same Backdrop CMS installation as the client module.

## Beta Release Notes
As a beta release, the core API endpoints, site registration, project version fetching, security review layer, and Views-based admin dashboard are fully functional. The module is stable enough for testing in development environments. Before deploying to production, ensure your Backdrop CMS environment is running PHP 8.0+ and clear your system caches after installation to register the API routes and admin menu items.

## Features

* **Site Registry:** Register each managed client site with a label, URL, and a unique API key. Keys are stored hashed at rest and displayed plaintext only once on generation. Keys are per-site revocable and regenerable without affecting other registered sites.
* **Automated Status Ingestion:** Accepts `POST /api/v1/remote-site-status/report` from client sites running the companion module. Each report carries Backdrop core version, PHP version, database driver and version, last cron time, and a full inventory of installed contrib projects (modules, themes, and layouts). Reports are stored and attributed to the sending site's key.
* **Authoritative Version Checking:** A manual "Fetch latest versions" action queries the Backdrop update server for each tracked project's release history. Latest stable and latest security versions are stored per project, alongside a `latest_checked_at` timestamp. Fetching is batch-processed and never runs automatically - the operator triggers it on demand.
* **Security Review Layer:** Per-project security status tracking (`Needs review` / `Reviewed`) driven by whether a security release exists that has not yet been stamped as reviewed. The operator can record security notes and mark a project reviewed directly from the admin UI. Not every security release requires an immediate update — a vulnerability may be irrelevant given how a particular site is configured or operated. The review layer allows the operator to acknowledge the release, document their assessment, and clear the flag so the security notice doesn't distract from releases where an immediate update is genuinely needed.
* **Two REST API Endpoints:** `POST /api/v1/remote-site-status/report` to ingest a site status report, and `GET /api/v1/remote-site-status/status` for health checks and connection testing. The `api/v1/remote-site-status/` namespace ensures these routes do not conflict with other API modules installed on the same server.
* **Views Integration:** Exposes all three data tables (sites, projects, site–project relationships) to the Backdrop Views module with full field, filter, sort, argument, and relationship support. Provides four default views: a registered sites list, a monitored projects list (with security status filter), a per-site project drill-down, and a per-project sites drill-down. Operators can build and export their own dashboard views using these as a starting point.
* **Version Highlighting:** Admin views colour-code project rows automatically — blue for an available update, red for a security release — using JavaScript that reads data attributes injected at render time. Dev version strings are excluded from comparisons.
* **Admin UI:** Full admin interface for managing registered sites (add, edit, regenerate key, revoke, reactivate, delete), reviewing project security status, triggering version fetches, and browsing the projects list with security filter.
* **Backdrop Native:** Built exclusively for Backdrop CMS using strict PHP 8.0+ standards and Backdrop APIs throughout.

## Requirements

- Backdrop CMS 1.x
- PHP 8.0+ (While the code may technically function with PHP 7.4 at this time, we strictly require PHP 8.0+ and will not address issues related to older PHP versions.)
- Companion module: [Remote Site Status Client](https://github.com/backdrop-contrib/remote_site_status_client) (installed on each managed client site)

## Installation

Install this module using the official Backdrop CMS instructions at https://docs.backdropcms.org/documentation/extend-with-modules

Enable the module on the site that will act as your monitoring server.

## Configuration

1. Navigate to **Admin → Configuration → Remote Site Status Server** to reach the main admin overview.
2. Click **Register a site** to add each managed client site. Provide a label and the site's URL.
3. Click **Generate API key** and copy the generated key — paste this into the corresponding `remote_site_status_client` settings on the client site. The key is shown only once.
4. Once client sites are reporting in, click **Projects** to see the tracked project list, then click **Fetch latest versions** to retrieve authoritative version data from the Backdrop update server.
5. Use the **Projects** list to review security status per project. Click **Review** on any project flagged as *Needs review* to read the security release details, add notes, and mark it reviewed.

Client sites should point their **Server base URL** to this installation (e.g. `https://status.example.com`) — the client module appends the API paths automatically.

## Issues

Bugs and feature requests should be reported in the Issue Queue: https://github.com/backdrop-contrib/remote_site_status_client/issues

## Current Maintainer(s)
- Steve Moorhouse (albanycomputers) (https://github.com/albanycomputers)
- Additional maintainers and contributors welcome.

## Planned Features

The following are on the roadmap but not yet implemented:

* **Version-aware filter handler:** A Views filter that compares installed version against latest version across the site–project relationship, so operators can build "show me all sites running an outdated version of X" views without writing custom SQL.
* **Stale cron and quiet site alerts:** Configurable thresholds (days since last cron / days since last report) surfaced as admin warnings and filterable in Views.
* **Security notification emails:** Automated emails to the operator when a project transitions to *Needs review*, with a configurable frequency to avoid alert fatigue.
* **Tier and access control:** Per-site tiers defining which features or reporting frequencies are available, consistent with the Webform Guard tier pattern.
* **Historical metrics:** A monthly snapshot table recording installed versions per site over time, enabling trend views and regression detection.

## Credits
- Steve Moorhouse — Zulip (DrAlbany)
- Claude Code by Anthropic assisted with development of this module.

- Current development is sponsored by [Albany Computer Services](https://www.albany-computers.co.uk), providers of computer support, [web design](https://www.albanywebdesign.co.uk), and [web hosting](https://www.albany-hosting.co.uk).

## License
This project is GPL v2 or later software. See the LICENSE.txt file in this directory for complete text.
