(function ($) {

  'use strict';

  /**
   * Copy a freshly minted API key from its show-once message to the clipboard.
   *
   * The key is shown only once, so the Copy button removes the risk of a
   * mis-selected manual copy. Binds once per button.
   */
  Backdrop.behaviors.remoteSiteStatusCopyKey = {
    attach: function (context) {
      $('.rstat-copy-key', context).once('rstat-copy-key', function () {
        var $button = $(this);

        $button.on('click', function () {
          var $field = $button.siblings('.rstat-key-value');
          var key = $field.val();
          // Update only the label span so the clipboard icon is preserved.
          var $label = $button.find('.rstat-copy-label');

          // Briefly confirm the copy, then restore the button label.
          var confirm = function () {
            var original = $label.text();
            $label.text(Backdrop.t('Copied'));
            window.setTimeout(function () {
              $label.text(original);
            }, 2000);
          };

          // The async Clipboard API is only available on secure origins
          // (HTTPS or localhost). On plain-HTTP dev sites it is undefined, so
          // fall back to selecting the field and execCommand('copy').
          if (window.navigator.clipboard && window.navigator.clipboard.writeText) {
            window.navigator.clipboard.writeText(key).then(confirm, function () {
              remoteSiteStatusFallbackCopy($field, confirm);
            });
          }
          else {
            remoteSiteStatusFallbackCopy($field, confirm);
          }
        });
      });
    }
  };

  /**
   * Clipboard fallback for browsers/origins without the async Clipboard API:
   * select the field and use the legacy execCommand('copy'). If even that
   * fails, the field is left selected so the operator can copy by hand.
   */
  function remoteSiteStatusFallbackCopy($field, confirm) {
    var field = $field.get(0);
    if (!field) {
      return;
    }
    field.focus();
    field.select();
    field.setSelectionRange(0, 99999);
    try {
      if (document.execCommand('copy')) {
        confirm();
      }
    }
    catch (e) {
      // Leave the field selected for a manual copy.
    }
  }

  /**
   * Compare two Backdrop version strings numerically.
   *
   * Strips the core-compat prefix ("1.x-") and any pre-release suffix before
   * comparing each numeric segment, so "1.x-1.10.0" correctly ranks above
   * "1.x-1.9.0" where a plain string compare would not.
   *
   * Returns a positive number if a > b, negative if a < b, 0 if equal.
   */
  function remoteSiteStatusCompareVersions(a, b) {
    function normalise(v) {
      var s = String(v).replace(/^\d+\.x-/, '');
      // Dev snapshots ("dev", "2.x-dev") are not stable releases; treat as
      // uncomparable so they never trigger an update or security highlight.
      if (/\bdev\b/i.test(s)) {
        return null;
      }
      return s.split('-')[0].split('.').map(Number);
    }
    var pa = normalise(a);
    var pb = normalise(b);
    if (!pa || !pb) {
      return 0;
    }
    for (var i = 0; i < Math.max(pa.length, pb.length); i++) {
      var diff = (pa[i] || 0) - (pb[i] || 0);
      if (diff !== 0) {
        return diff;
      }
    }
    return 0;
  }

  /**
   * Colour the project version red when it is itself a security release.
   *
   * Reads data attributes on the rstat-project-version span written by the
   * sites-with-projects view rewrite: data-latest, data-security. When the
   * security version is present and equal to (or newer than) the latest version
   * the project version label is coloured red to signal urgency.
   */
  Backdrop.behaviors.remoteSiteStatusProjectVersionHighlight = {
    attach: function (context) {
      $('.rstat-project-version', context).once('rstat-project-version', function () {
        var $span    = $(this);
        var latest   = $span.data('latest');
        var security = $span.data('security');
        if (security && latest && remoteSiteStatusCompareVersions(security, latest) >= 0) {
          $span.addClass('rstat-security-release');
        }
      });
    }
  };

  /**
   * Colour the header version red on the sites-per-project view when the
   * latest release is a security release.
   *
   * The header uses a Views token ([latest_version]) which resolves to rendered
   * field output, making data-attribute injection unreliable. Instead, PHP
   * passes the raw version values as Backdrop.settings.remoteSiteStatusHeader
   * and this behaviour reads them to apply the highlight without touching the
   * header markup.
   */
  Backdrop.behaviors.remoteSiteStatusHeaderHighlight = {
    attach: function (context) {
      var settings = Backdrop.settings.remoteSiteStatusHeader;
      if (!settings || !settings.latest || !settings.security) {
        return;
      }
      $('.view-header', context).once('rstat-header-highlight', function () {
        if (remoteSiteStatusCompareVersions(settings.security, settings.latest) >= 0) {
          $(this).find('.rstat-version, .rstat-project-version').first()
            .addClass('rstat-security-release');
        }
      });
    }
  };

  /**
   * Highlight module rows where installed version is behind latest or security.
   *
   * Reads data attributes written by the Views "installed_version" field
   * rewrite: data-installed, data-latest, data-security. Security takes
   * priority and marks the row red; out-of-date-only marks it blue.
   */
  Backdrop.behaviors.remoteSiteStatusVersionHighlight = {
    attach: function (context) {
      $('.rstat-version', context).once('rstat-version-highlight', function () {
        var $span     = $(this);
        var installed = $span.data('installed');
        var latest    = $span.data('latest');
        var security  = $span.data('security');
        var $row      = $span.closest('tr');

        if (!installed) {
          return;
        }
        if (security && remoteSiteStatusCompareVersions(security, installed) > 0) {
          $row.addClass('rstat-security-update');
        }
        else if (latest && remoteSiteStatusCompareVersions(latest, installed) > 0) {
          $row.addClass('rstat-outdated');
        }
      });
    }
  };

})(jQuery);
