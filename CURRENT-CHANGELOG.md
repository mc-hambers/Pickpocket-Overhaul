# Pickpocket Chance Rework 1.1.3

Changes since 1.1.0:

- Fixed chance hooks on protected game executables and when another mod redirects the supported call sites.
- Fixed equipped-item drop crime routing; the feature still follows your existing MCM toggle.
- Reuse environment and witness calculations within a frame while keeping actual theft rolls fresh.
- Batch stolen-inventory checks and reduce synchronous logging to cut repeated work.
- Preserved chance rules, settings and existing save data.

Existing saves are supported; no new game or save cleaner is required. Build and automated checks passed. These checks do not prove every gameplay path or hitch improvement.
