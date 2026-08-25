"""Generate Commentarium dialogue data from the editable text source."""

from pathlib import Path
import sys

MAX_LINE_LENGTH = 21
MAX_COMMENT_LENGTH = MAX_LINE_LENGTH * 3


def wrap_comment(comment: str) -> list[str]:
    words = comment.split()
    lines: list[str] = []
    current = ""

    for word in words:
        if len(word) > MAX_LINE_LENGTH:
            raise ValueError(f"word exceeds {MAX_LINE_LENGTH} characters: {word!r}")
        candidate = word if not current else f"{current} {word}"
        if len(candidate) <= MAX_LINE_LENGTH:
            current = candidate
        else:
            lines.append(current)
            current = word
    if current:
        lines.append(current)
    if len(lines) > 3:
        # The source contract is 63 characters, not three whole-word lines.
        # Retain a valid comment by falling back to exact 21-character chunks;
        # only these unusually packed entries break a word at the display edge.
        lines = [
            comment[0:MAX_LINE_LENGTH],
            comment[MAX_LINE_LENGTH:MAX_LINE_LENGTH * 2],
            comment[MAX_LINE_LENGTH * 2:MAX_COMMENT_LENGTH],
        ]
    return lines + [""] * (3 - len(lines))


def c_string(value: str) -> str:
    return '"' + value.replace("\\", "\\\\").replace('"', '\\"') + '"'


def main() -> int:
    root = Path(__file__).resolve().parent.parent
    source = root / "txt" / "dumbcomments.txt"
    output = root / "src" / "dumb_comments_data.h"
    comments = [line.strip() for line in source.read_text(encoding="utf-8").splitlines() if line.strip()]

    if not comments:
        raise ValueError("dumbcomments.txt does not contain any comments")
    if len(comments) > 255:
        raise ValueError("Commentarium supports at most 255 comments")

    formatted = []
    for number, comment in enumerate(comments, start=1):
        if len(comment) > MAX_COMMENT_LENGTH:
            raise ValueError(
                f"comment {number} is {len(comment)} characters; maximum is {MAX_COMMENT_LENGTH}: {comment!r}"
            )
        formatted.append(wrap_comment(comment))

    lines = [
        "/* Generated from txt/dumbcomments.txt. Do not edit directly. */",
        "#ifndef DUMB_COMMENTS_DATA_H",
        "#define DUMB_COMMENTS_DATA_H",
        "",
        f"#define DUMB_COMMENT_COUNT {len(formatted)}",
        "",
        "typedef struct {",
        "    const char *line1;",
        "    const char *line2;",
        "    const char *line3;",
        "} DumbComment;",
        "",
        "static const DumbComment dumb_comments[DUMB_COMMENT_COUNT] = {",
    ]
    for wrapped in formatted:
        lines.append("    { " + ", ".join(c_string(line) for line in wrapped) + " },")
    lines.extend(["};", "", "#endif", ""])
    output.write_text("\n".join(lines), encoding="ascii")
    print(f"Wrote {output} ({len(formatted)} comments)")
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError) as error:
        print(f"dumb comment generation failed: {error}", file=sys.stderr)
        raise SystemExit(1)
