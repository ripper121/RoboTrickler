(function () {
  const fallbackLanguage = 'en';

  function isLocal() {
    return location.hostname === 'localhost' || location.hostname === '127.0.0.1' || location.hostname === '';
  }

  function browserLanguage() {
    // file:// has no access to config.txt, so use the browser's preferred
    // language. Normalize like the firmware does (e.g. "de-DE" -> "de"); an
    // unsupported language simply falls back to English when its file is missing.
    try {
      const preferred = (navigator.languages && navigator.languages[0]) || navigator.language;
      if (preferred) {
        return String(preferred).split('-')[0].toLowerCase();
      }
    } catch (e) {
    }
    return '';
  }

  function localLanguage() {
    try {
      const saved = window.localStorage && localStorage.getItem('rt_language');
      if (saved) {
        return saved;
      }
    } catch (e) {
    }
    return browserLanguage() || fallbackLanguage;
  }

  function languageUrl(language) {
    // A local file:// page cannot fetch() a JSON file (blocked origin), so load
    // a JSONP wrapper (<lang>.js) via <script> injection instead. When served by
    // the webserver we fetch the (gzip-compressed) JSON as before.
    if (isLocal()) {
      return './lang/' + language + '.js';
    }
    return '/system/lang/' + language + '.json';
  }

  function textFromPath(data, path) {
    return path.split('.').reduce(function (value, key) {
      return value && value[key] !== undefined ? value[key] : undefined;
    }, data);
  }

  function applyTranslations(data) {
    document.documentElement.lang = data.language || fallbackLanguage;
    document.querySelectorAll('[data-i18n]').forEach(function (node) {
      const value = textFromPath(data.web || {}, node.dataset.i18n);
      if (value !== undefined) {
        node.textContent = value;
      }
    });
    document.querySelectorAll('[data-i18n-placeholder]').forEach(function (node) {
      const value = textFromPath(data.web || {}, node.dataset.i18nPlaceholder);
      if (value !== undefined) {
        node.setAttribute('placeholder', value);
      }
    });
    document.querySelectorAll('[data-i18n-title]').forEach(function (node) {
      const value = textFromPath(data.web || {}, node.dataset.i18nTitle);
      if (value !== undefined) {
        node.setAttribute('title', value);
      }
    });
    window.rtLang = data.web || {};
    window.rtT = function (path, fallback) {
      const value = textFromPath(window.rtLang, path);
      return value !== undefined ? value : fallback;
    };
  }

  function loadViaFetch(url) {
    return fetch(url)
      .then(function (response) {
        if (!response.ok) {
          throw new Error('language not found');
        }
        return response.json();
      })
      .then(function (data) {
        applyTranslations(data);
      });
  }

  function loadViaScript(url) {
    // The injected JSONP file calls window.rtLoadLanguage(data) when it runs.
    return new Promise(function (resolve, reject) {
      const script = document.createElement('script');
      let settled = false;
      window.rtLoadLanguage = function (data) {
        settled = true;
        try {
          applyTranslations(data);
          resolve();
        } catch (e) {
          reject(e);
        }
      };
      script.onerror = function () {
        if (!settled) {
          reject(new Error('language not found'));
        }
      };
      script.src = url;
      document.head.appendChild(script);
    });
  }

  function loadLanguage(language) {
    const url = languageUrl(language);
    return isLocal() ? loadViaScript(url) : loadViaFetch(url);
  }

  function activeLanguage() {
    if (isLocal()) {
      return Promise.resolve(localLanguage());
    }
    return fetch('/getLanguage')
      .then(function (response) {
        return response.ok ? response.text() : fallbackLanguage;
      })
      .then(function (language) {
        language = (language || fallbackLanguage).trim();
        return language || fallbackLanguage;
      })
      .catch(function () {
        return fallbackLanguage;
      });
  }

  function initializeLanguage() {
    return activeLanguage()
      .then(loadLanguage)
      .catch(function () {
        return loadLanguage(fallbackLanguage);
      });
  }

  document.addEventListener('DOMContentLoaded', function () {
    window.rtLanguageReady = initializeLanguage();
  });
}());
