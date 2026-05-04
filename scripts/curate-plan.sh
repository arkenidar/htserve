#!/usr/bin/env bash
# curate-plan.sh — copy a Claude plan markdown file into docs/plans/, render to
# HTML using a small built-in markdown subset, and regenerate the plans index.
#
# Usage:
#   curate-plan.sh <source.md> <slug> [--title "Title"]
#   curate-plan.sh --rebuild         # re-render every existing docs/plans/*.md
#   curate-plan.sh --help
#
# Idempotent: re-running on the same slug overwrites without duplicating index
# entries.

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
PLANS_DIR="$ROOT/docs/plans"
TEMPLATE="$PLANS_DIR/template.html"

usage() {
    sed -n '2,12p' "$0" | sed 's/^# \{0,1\}//'
    exit "${1:-0}"
}

# --- minimal markdown -> HTML renderer --------------------------------------
# Handles: # ## ### headings, ``` fenced code, `inline code`, **bold**,
# [text](href) links, - bullet lists, | tables (simple), blockquotes,
# blank-line paragraphs. Sufficient for plan markdown.
render_md() {
    awk '
    function esc(s) {
        gsub(/&/, "\\&amp;", s)
        gsub(/</, "\\&lt;", s)
        gsub(/>/, "\\&gt;", s)
        return s
    }
    function inline(s,    out, i, c, n, m, link, txt, href) {
        out = ""
        n = length(s)
        i = 1
        while (i <= n) {
            c = substr(s, i, 1)
            if (c == "`") {
                m = index(substr(s, i+1), "`")
                if (m > 0) {
                    out = out "<code>" esc(substr(s, i+1, m-1)) "</code>"
                    i = i + m + 1
                    continue
                }
            }
            if (c == "[") {
                # match [text](href)
                rest = substr(s, i)
                if (match(rest, /^\[[^]]*\]\([^)]*\)/)) {
                    link = substr(rest, 1, RLENGTH)
                    rb = index(link, "]")
                    txt = substr(link, 2, rb-2)
                    href = substr(link, rb+2, length(link)-rb-2)
                    out = out "<a href=\"" esc(href) "\">" esc(txt) "</a>"
                    i = i + RLENGTH
                    continue
                }
            }
            if (c == "*" && substr(s, i+1, 1) == "*") {
                rest = substr(s, i+2)
                m = index(rest, "**")
                if (m > 0) {
                    out = out "<strong>" esc(substr(rest, 1, m-1)) "</strong>"
                    i = i + 2 + m + 1
                    continue
                }
            }
            out = out esc(c)
            i++
        }
        return out
    }
    BEGIN { in_code=0; list_kind=""; in_table=0; para="" }
    function flush_para() {
        if (para != "") {
            print "<p>" inline(para) "</p>"
            para = ""
        }
    }
    function close_list() {
        if (list_kind != "") { print "</" list_kind ">"; list_kind="" }
    }
    function close_table() {
        if (in_table) { print "</table>"; in_table=0 }
    }
    {
        line = $0
        # fenced code block (allow leading whitespace)
        if (line ~ /^[ \t]*```/) {
            flush_para(); close_list(); close_table()
            if (in_code) { print "</code></pre>"; in_code=0 }
            else { print "<pre><code>"; in_code=1 }
            next
        }
        if (in_code) { print esc(line); next }

        # heading
        if (match(line, /^(#{1,6}) /)) {
            flush_para(); close_list(); close_table()
            level = RLENGTH - 1
            content = substr(line, RLENGTH + 1)
            print "<h" level ">" inline(content) "</h" level ">"
            next
        }

        # bullet list
        if (line ~ /^[ ]*- /) {
            flush_para(); close_table()
            if (list_kind != "ul") { close_list(); print "<ul>"; list_kind="ul" }
            sub(/^[ ]*- /, "", line)
            print "<li>" inline(line) "</li>"
            next
        }
        # numbered list
        if (line ~ /^[ ]*[0-9]+\. /) {
            flush_para(); close_table()
            if (list_kind != "ol") { close_list(); print "<ol>"; list_kind="ol" }
            sub(/^[ ]*[0-9]+\. /, "", line)
            print "<li>" inline(line) "</li>"
            next
        }
        close_list()

        # table (simple pipe table)
        if (line ~ /^\|/) {
            flush_para()
            # detect separator row (---|---) -> skip
            if (line ~ /^\|[ \-:|]+\|$/) { next }
            if (!in_table) { print "<table>"; in_table=1; rownum=0 }
            rownum++
            tag = (rownum == 1) ? "th" : "td"
            n = split(line, cells, "|")
            row = "<tr>"
            for (k=2; k<n; k++) {
                cell = cells[k]
                gsub(/^ +| +$/, "", cell)
                row = row "<" tag ">" inline(cell) "</" tag ">"
            }
            row = row "</tr>"
            print row
            next
        } else {
            close_table()
        }

        # blockquote
        if (line ~ /^> /) {
            flush_para()
            sub(/^> /, "", line)
            print "<blockquote>" inline(line) "</blockquote>"
            next
        }

        # blank line
        if (line ~ /^[ ]*$/) {
            flush_para()
            next
        }

        # paragraph accumulation
        if (para == "") para = line
        else para = para " " line
    }
    END {
        flush_para(); close_list(); close_code=in_code
        if (in_code) print "</code></pre>"
        close_table()
    }
    '
}

extract_title() {
    awk '/^# / { sub(/^# /, ""); print; exit }' "$1"
}

curate_one() {
    local src="$1" slug="$2" title="${3:-}"

    [[ -f "$src" ]] || { echo "source not found: $src" >&2; exit 1; }
    [[ -f "$TEMPLATE" ]] || { echo "template missing: $TEMPLATE" >&2; exit 1; }

    mkdir -p "$PLANS_DIR"
    local md_dst="$PLANS_DIR/$slug.md"
    local html_dst="$PLANS_DIR/$slug.html"

    # copy source unless destination is already the source (rebuild case)
    if [[ "$(realpath "$src")" != "$(realpath -m "$md_dst")" ]]; then
        cp "$src" "$md_dst"
    fi

    if [[ -z "$title" ]]; then
        title="$(extract_title "$md_dst")"
        [[ -n "$title" ]] || title="$slug"
    fi

    local body
    body="$(render_md < "$md_dst")"

    # Substitute into template. Use perl-free approach: sed for title, awk for body.
    awk -v title="$title" -v body="$body" '
        { gsub(/__TITLE__/, title); }
        /<!--CONTENT-->/ { print body; next }
        { print }
    ' "$TEMPLATE" > "$html_dst"

    echo "wrote $md_dst"
    echo "wrote $html_dst"
}

rebuild_index() {
    mkdir -p "$PLANS_DIR"
    local index="$PLANS_DIR/index.html"
    {
        cat <<'HEADER'
<!doctype html>
<html lang="en">
<meta charset="utf-8">
<title>htserve plans</title>
<style>
  body { font-family: system-ui, sans-serif; max-width: 760px; margin: 2em auto; padding: 0 1em; line-height: 1.5; color: #222; }
  nav a { margin-right: 1em; }
  ul { padding-left: 1.2em; }
  li { margin: 0.4em 0; }
  small { color: #777; }
</style>

<nav><a href="../index.html">&larr; Docs home</a></nav>

<h1>Plans</h1>
<p>Curated implementation plans for htserve, newest first. Plans capture the
<em>why</em> behind a change — context that doesn't fit in commits or code.
See the project README for the curation workflow.</p>

<ul>
HEADER

        # List *.md files in PLANS_DIR by mtime, newest first
        find "$PLANS_DIR" -maxdepth 1 -name '*.md' -printf '%T@ %f\n' \
            | sort -rn \
            | while read -r _ts fname; do
                local slug="${fname%.md}"
                local title
                title="$(extract_title "$PLANS_DIR/$fname")"
                [[ -n "$title" ]] || title="$slug"
                # HTML-escape the title minimally
                title="${title//&/&amp;}"
                title="${title//</&lt;}"
                title="${title//>/&gt;}"
                printf '  <li><a href="%s.html">%s</a> <small>(<a href="%s.md">md</a>)</small></li>\n' \
                    "$slug" "$title" "$slug"
            done

        cat <<'FOOTER'
</ul>
</html>
FOOTER
    } > "$index"
    echo "wrote $index"
}

# --- main -------------------------------------------------------------------
case "${1:-}" in
    -h|--help|"") usage 0 ;;
    --rebuild)
        for md in "$PLANS_DIR"/*.md; do
            [[ -f "$md" ]] || continue
            slug="$(basename "$md" .md)"
            curate_one "$md" "$slug"
        done
        rebuild_index
        ;;
    *)
        src="$1"; slug="${2:-}"; shift 2 || true
        title=""
        while [[ $# -gt 0 ]]; do
            case "$1" in
                --title) title="$2"; shift 2 ;;
                *) echo "unknown arg: $1" >&2; exit 1 ;;
            esac
        done
        [[ -n "$slug" ]] || { echo "missing <slug>" >&2; usage 1; }
        curate_one "$src" "$slug" "$title"
        rebuild_index
        ;;
esac
