(function () {
  const fallbackLanguage = 'en';

  function localLanguage() {
    try {
      const saved = window.localStorage && localStorage.getItem('rt_language');
      if (saved) {
        return saved;
      }
    } catch (e) {
    }
    return fallbackLanguage;
  }

  function languageUrl(language) {
    if (location.hostname === 'localhost' || location.hostname === '127.0.0.1' || location.hostname === '') {
      return './lang/' + language + '.json';
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

  function loadLanguage(language) {
    return fetch(languageUrl(language))
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

  function activeLanguage() {
    if (location.hostname === 'localhost' || location.hostname === '127.0.0.1' || location.hostname === '') {
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

  document.addEventListener('DOMContentLoaded', function () {
    activeLanguage()
      .then(loadLanguage)
      .catch(function () {
        return loadLanguage(fallbackLanguage);
      });
  });
}());
