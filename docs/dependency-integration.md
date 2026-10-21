# Structured output dependency integration

FetchContent defaults:

| Dependency | Repository this fork fetches | Revision |
| --- | --- | --- |
| XGrammar | `https://github.com/DassaultFalconKing/xgrammar.git` | `5b0b7084a65a2059b44c03fbc6320ad7fcc41f33` |
| Minja | `https://github.com/google/minja.git` | `021c2293c187789ef13d56c6cfd89c9b134fd80f` |

XGrammar upstream base remains `mlc-ai/xgrammar` `614ab01d2af3fdfdde73ae3e4171a7f36db45c23`.
`5b0b708` is that base plus the published fork commits through #802, then the XML end-marker lock:

- `0cd00f5` empty `ExcludeToken()` parser fix
- `c77d5e0` cache options in `cmake/config.cmake` without rewriting source
- `3303659` / `b288dca` property-less Qwen XML epsilon and the R1 required/minProperties guard
- `5b0b708` XML #650: `lock_excluded_prefixes` on the `xml_string` TagDispatch, so a started `</parameter` cannot continue as ordinary content

The undeclared-required empty-object residual is unchanged. Minja in this fork is still the upstream base; the application cohort still needs its Minja compatibility series via `FETCHCONTENT_SOURCE_DIR_MINJA`. This pin is source only: it has not been compiled into a DLL, it is not runtime acceptance, and it does not revise the public cohort lock.

For a local cohort, provide patched checkouts through CMake's existing overrides:

```text
-DFETCHCONTENT_SOURCE_DIR_XGRAMMAR=<absolute-path-to-patched-xgrammar>
-DFETCHCONTENT_SOURCE_DIR_MINJA=<absolute-path-to-patched-minja>
```

Record the exact patched revisions and paths in the cohort lock. GenAI does not
rewrite a fetched XGrammar checkout. The XGrammar default-options regression is
available with `cmake -P tests/cmake/test_config_cache.cmake` in that component.

This source integration does not establish native build, binding, GPU, or runtime
acceptance. Run those checks against the complete locked cohort.
