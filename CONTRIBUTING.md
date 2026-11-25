# Contributing to ezQuake

Thanks for taking the time to contribute! Every thoughtful bug report,
documentation improvement, and code change makes this project better.

Repository: [ezQuake](https://github.com/qw-group/ezquake-source).

Target branch: master.

## Development setup

1. Fork the repository and clone your fork.
2. Setup your build environment, see [BUILD.md](BUILD.md).
3. Create a branch for your change.

## Testing your changes

Run the checks relevant to your change before opening a pull request. The build
system provides a ASan+UBSan preset, make use of it to reduce risk of
unintentional errors.

## Reporting issues

Search existing issues first. For a bug report, include steps to reproduce,
expected and actual behavior, and your environment. Discuss substantial
changes before starting work.

For security related issues, see [SECURITY.md](SECURITY.md).

## Pull requests

Keep pull requests focused on one change. Explain the problem, your approach,
and how you tested it. Link any related issues and respond to review feedback
constructively.

* If master has moved, rebase your branch, never introduce a merge commit.
* Each commit should compile on its own so that `git bisect` remains usable.
* Each commit should add value to future commits on its own to make reviews more
  readable (*"A, then B, then C which makes use of A + B"* type storytelling).
* No free LLM advertisement via *Co-Authored-By* in commits or PR descriptions.
* Make use of your LLM subscription or gamer GPU for a `/code-review` before opening a PR.

## Coding conventions

Follow the naming, formatting, and patterns already used in the project. Avoid
unrelated changes such as reformating lines of code or changing file ending.
The code base is more than 30 years old so style differs a bit here and there.
When adding new code, make use of [.clang-format](.clang-format). For changes
to old functions, follow the style in that specific function.

## Beyond code

Code is only one way to help. You can improve documentation, reproduce an issue,
suggest clearer wording, or help someone get started.
