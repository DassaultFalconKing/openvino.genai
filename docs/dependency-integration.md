# Structured output dependency integration

The portable FetchContent defaults use the locked upstream bases:

| Dependency | Upstream repository | Base revision |
| --- | --- | --- |
| XGrammar | `https://github.com/mlc-ai/xgrammar.git` | `614ab01d2af3fdfdde73ae3e4171a7f36db45c23` |
| Minja | `https://github.com/google/minja.git` | `021c2293c187789ef13d56c6cfd89c9b134fd80f` |

The structured token bridge requires the companion XGrammar empty
`ExcludeToken()` parser fix. Embedding GenAI without modifying dependency source
also requires the companion XGrammar change that honors cache options in
`cmake/config.cmake`. The application cohort additionally requires its Minja
compatibility patch series. These component changes must accompany a production
integration; the upstream base pins alone do not represent the patched cohort.
The GenAI pull request must declare dependencies on the corresponding component
pull requests.

For a local cohort, provide patched checkouts through CMake's existing overrides:

```text
-DFETCHCONTENT_SOURCE_DIR_XGRAMMAR=<absolute-path-to-patched-xgrammar>
-DFETCHCONTENT_SOURCE_DIR_MINJA=<absolute-path-to-patched-minja>
```

Record the exact patched revisions and paths in the cohort lock. The public
upstream URLs remain fetchable, and no unpublished fork commit is advertised as
fetchable from an upstream repository. GenAI sets XGrammar's cache options and
does not rewrite a fetched checkout. The XGrammar default-options regression is
available with `cmake -P tests/cmake/test_config_cache.cmake` in that component.

This source integration does not establish native build, binding, GPU, or runtime
acceptance. Run those checks against the complete locked cohort.
