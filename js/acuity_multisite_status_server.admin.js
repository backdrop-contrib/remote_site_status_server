(function ($) {

  'use strict';

  /**
   * Copy a freshly minted API key from its show-once message to the clipboard.
   *
   * The key is shown only once, so the Copy button removes the risk of a
   * mis-selected manual copy. Binds once per button.
   */
  Backdrop.behaviors.acuityMultisiteStatusCopyKey = {
    attach: function (context) {
      $('.acuity-copy-key', context).once('acuity-copy-key', function () {
        var $button = $(this);

        $button.on('click', function () {
          var $field = $button.siblings('.acuity-key-value');
          var key = $field.val();
          // Update only the label span so the clipboard icon is preserved.
          var $label = $button.find('.acuity-copy-label');

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
              acuityMultisiteStatusFallbackCopy($field, confirm);
            });
          }
          else {
            acuityMultisiteStatusFallbackCopy($field, confirm);
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
  function acuityMultisiteStatusFallbackCopy($field, confirm) {
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

})(jQuery);
