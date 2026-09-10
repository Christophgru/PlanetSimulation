When editing Markdown files:

- NEVER use triple-backtick Markdown code fences inside Markdown files.
- Use tilde fences instead, for example ~~~bash ... ~~~.
- Do not interpret shell commands, source-code lines, documentation headings,
  tree listings, or prose as filenames.
- Only create a new file when the task explicitly requires that file.
- Before creating any file, verify that the proposed filename looks like an
  intentional project path.