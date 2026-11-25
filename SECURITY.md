## Reporting a Vulnerability

**Do not open public issues for new vulnerabilities**.

Instead, use the **Report a vulnerability** button on the
[repository's Security tab](https://github.com/QW-Group/ezquake-source/security).
That opens a private advisory that only the maintainers can see, and we can
work on the fix with you there.

If you prefer email, check git log for email to people with merge access.

Include clear steps to reproduce and details about the potential impact.

If the vulnerability is triggered by a specific binary sequence during asset or
network parsing, try to avoid sharing that byte sequence alone as it requires a more
strict environment to verify. Instead share it together with a high level script
that generates that same byte sequence that is easy to review.
