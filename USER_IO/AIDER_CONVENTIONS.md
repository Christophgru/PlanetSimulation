When editing Markdown files:

- NEVER use triple-backtick Markdown code fences inside Markdown files.
- Use tilde fences instead, for example ~~~bash ... ~~~.
- Do not interpret shell commands, source-code lines, documentation headings,
  tree listings, or prose as filenames.
- Only create a new file when the task explicitly requires that file.
- Before creating any file, verify that the proposed filename looks like an
  intentional project path.



Agent progress rules:
- Never weaken, delete or modify an existing passing test to make production code pass.
- Fix at most one failing test or one explicitly requested subsystem per iteration.
- If reasoning begins repeating the same conclusion, stop rather than continuing speculative changes.
- A regression in the number of passing tests is not progress; revert the responsible change.
