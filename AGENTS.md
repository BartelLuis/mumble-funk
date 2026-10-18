# Instructions for Codex in Shawna7han/mumble-funk

## Git commit identity

For every new commit in this repository, use:

- Author and committer name: `Shawna7han`
- Email: `343870343+Shawna7han@users.noreply.github.com`

This is the repository owner's numeric GitHub account ID and username in GitHub's
private No-Reply format. In each fresh checkout, set repository-local configuration
before committing; do not change the global identity of unrelated repositories:

```bash
git config --local user.name Shawna7han
git config --local user.email 343870343+Shawna7han@users.noreply.github.com
```

Check the effective author and committer with `git var GIT_AUTHOR_IDENT` and
`git var GIT_COMMITTER_IDENT`. Ensure environment overrides do not select a
different identity. Preserve all existing commits and their authorship: do not
amend, rebase, force-push or rewrite history to change an old identity.

This instruction is intended to persist across future Codex tasks and clones.
