#!/usr/bin/env python3
"""Render the latest CTest and gcovr result as a static Pages dashboard."""

from __future__ import annotations

import argparse
from collections import defaultdict
from collections.abc import Mapping
from dataclasses import dataclass
from datetime import datetime, timezone
from html import escape
import json
from pathlib import Path
from typing import Any
import xml.etree.ElementTree as ElementTree


METRIC_NAMES = ("line", "branch", "function")
METRIC_ALIASES = {
    "line": ("line", "lines"),
    "branch": ("branch", "branches"),
    "function": ("function", "functions"),
}


@dataclass
class Metric:
    covered: int = 0
    total: int = 0
    available: bool = False

    def add(self, other: "Metric") -> None:
        if other.available:
            self.covered += other.covered
            self.total += other.total
            self.available = True


@dataclass
class TestCase:
    name: str
    status: str
    duration: float | None
    detail: str


@dataclass
class TestSummary:
    cases: list[TestCase]
    unavailable_reason: str | None = None

    @property
    def passed(self) -> int:
        return sum(case.status == "PASS" for case in self.cases)

    @property
    def failed(self) -> int:
        return sum(case.status == "FAIL" for case in self.cases)

    @property
    def skipped(self) -> int:
        return sum(case.status == "SKIP" for case in self.cases)

    @property
    def total(self) -> int:
        return len(self.cases)


BASE_CSS = """
:root {
  color-scheme: light;
  font-family: Inter, ui-sans-serif, system-ui, -apple-system, BlinkMacSystemFont, "Segoe UI", sans-serif;
  color: #1c2733;
  background: #f4f6f7;
}

* { box-sizing: border-box; }

body {
  margin: 0;
  background: #f4f6f7;
  color: #1c2733;
  line-height: 1.5;
}

a { color: #076b7a; text-decoration: none; }
a:hover { text-decoration: underline; }

.frame {
  width: min(1180px, calc(100% - 32px));
  margin: 0 auto;
}

.site-header {
  background: #ffffff;
  border-bottom: 1px solid #d7dde0;
}

.site-header .frame {
  min-height: 62px;
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 24px;
}

.brand {
  color: #1c2733;
  font-weight: 700;
  font-size: 18px;
  white-space: nowrap;
}

.brand:hover { text-decoration: none; }

nav { display: flex; align-self: stretch; align-items: center; gap: 20px; }
nav a {
  display: inline-flex;
  align-items: center;
  border-bottom: 2px solid transparent;
  color: #52606d;
  font-size: 14px;
  font-weight: 600;
}
nav a.active { border-bottom-color: #087f5b; color: #1c2733; }

main { padding: 36px 0 48px; }

.eyebrow {
  margin: 0 0 8px;
  color: #52606d;
  font-size: 13px;
  font-weight: 700;
  text-transform: uppercase;
}

h1, h2, h3, p { margin-top: 0; }
h1 { margin-bottom: 8px; font-size: 32px; line-height: 1.2; }
h2 { margin-bottom: 14px; font-size: 20px; line-height: 1.3; }
h3 { margin-bottom: 4px; font-size: 16px; }

.subtitle { color: #52606d; margin-bottom: 0; }

.status-panel {
  display: flex;
  align-items: center;
  justify-content: space-between;
  gap: 20px;
  margin: 28px 0;
  padding: 18px 20px;
  border: 1px solid #d7dde0;
  border-left-width: 6px;
  border-radius: 6px;
  background: #ffffff;
}

.status-panel.pass { border-left-color: #087f5b; }
.status-panel.fail { border-left-color: #c52b2f; }
.status-title { margin: 0; font-size: 20px; font-weight: 750; }
.status-copy { margin: 4px 0 0; color: #52606d; }

.button-link {
  display: inline-flex;
  align-items: center;
  min-height: 34px;
  padding: 6px 10px;
  border: 1px solid #9aabb3;
  border-radius: 4px;
  color: #1c2733;
  font-size: 14px;
  font-weight: 650;
  white-space: nowrap;
}
.button-link:hover { background: #edf2f3; text-decoration: none; }

.metric-grid {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(160px, 1fr));
  gap: 12px;
  margin: 20px 0 32px;
}

.metric-card {
  min-height: 118px;
  padding: 16px;
  border: 1px solid #d7dde0;
  border-radius: 6px;
  background: #ffffff;
}

.metric-label { color: #52606d; font-size: 13px; font-weight: 650; }
.metric-value { margin-top: 8px; font-size: 26px; font-weight: 760; line-height: 1.1; }
.metric-detail { margin-top: 7px; color: #52606d; font-size: 13px; }
.metric-card.fail .metric-value { color: #c52b2f; }
.metric-card.pass .metric-value { color: #087f5b; }

.section { margin-top: 34px; }
.scope-note { color: #52606d; max-width: 820px; }

.table-wrap {
  overflow-x: auto;
  border: 1px solid #d7dde0;
  border-radius: 6px;
  background: #ffffff;
}

table { width: 100%; border-collapse: collapse; font-size: 14px; }
th, td { padding: 11px 14px; border-bottom: 1px solid #e3e8ea; text-align: left; vertical-align: top; }
th { color: #52606d; background: #f8fafb; font-size: 12px; font-weight: 700; text-transform: uppercase; }
tbody tr:last-child td { border-bottom: 0; }
tbody tr:hover { background: #f8fafb; }

.coverage-cell { font-variant-numeric: tabular-nums; white-space: nowrap; }
.coverage-cell span { display: block; color: #52606d; font-size: 12px; }
.status-pass { color: #087f5b; font-weight: 700; }
.status-fail { color: #c52b2f; font-weight: 700; }
.status-skip { color: #8b5a00; font-weight: 700; }

.metadata {
  display: grid;
  grid-template-columns: repeat(auto-fit, minmax(220px, 1fr));
  gap: 10px 22px;
  margin: 18px 0 0;
  padding: 0;
  list-style: none;
}
.metadata dt { color: #52606d; font-size: 13px; }
.metadata dd { margin: 2px 0 0; overflow-wrap: anywhere; }
code { font-family: ui-monospace, SFMono-Regular, Menlo, Consolas, monospace; font-size: 0.92em; }

details { border-top: 1px solid #e3e8ea; padding: 12px 0; }
details:first-of-type { border-top: 0; }
summary { cursor: pointer; color: #1c2733; font-weight: 650; }
pre {
  margin: 10px 0 0;
  padding: 12px;
  overflow-x: auto;
  border: 1px solid #d7dde0;
  border-radius: 4px;
  background: #f8fafb;
  color: #2f3b45;
  font: 12px/1.45 ui-monospace, SFMono-Regular, Menlo, Consolas, monospace;
  white-space: pre-wrap;
  overflow-wrap: anywhere;
}

.notice {
  margin: 20px 0;
  padding: 12px 14px;
  border-left: 4px solid #b7791f;
  background: #fffaf0;
  color: #624a13;
}

.empty { color: #52606d; }
.footer { margin-top: 42px; color: #697780; font-size: 13px; }

@media (max-width: 620px) {
  .site-header .frame { min-height: 56px; gap: 12px; }
  nav { gap: 12px; }
  nav a { font-size: 13px; }
  h1 { font-size: 26px; }
  .status-panel { align-items: flex-start; flex-direction: column; }
  .button-link { white-space: normal; }
}
"""


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--junit", type=Path, required=True)
    parser.add_argument("--coverage-summary", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    parser.add_argument("--source-root", type=Path, required=True)
    parser.add_argument("--test-status", choices=("PASS", "FAIL"), required=True)
    parser.add_argument("--repository-sha", required=True)
    parser.add_argument("--upstream-sha", required=True)
    parser.add_argument("--run-url", required=True)
    return parser.parse_args()


def local_name(tag: str) -> str:
    return tag.rsplit("}", 1)[-1]


def parse_duration(value: str | None) -> float | None:
    if value is None:
        return None
    try:
        return float(value)
    except ValueError:
        return None


def detail_from_element(element: ElementTree.Element) -> str:
    message = element.attrib.get("message", "").strip()
    body = "".join(element.itertext()).strip()
    return "\n".join(part for part in (message, body) if part)


def parse_junit(path: Path) -> TestSummary:
    if not path.exists():
        return TestSummary([], "CTest did not produce a JUnit XML file.")

    try:
        root = ElementTree.parse(path).getroot()
    except ElementTree.ParseError as error:
        return TestSummary([], f"CTest JUnit XML could not be parsed: {error}")

    cases: list[TestCase] = []
    for element in root.iter():
        if local_name(element.tag) != "testcase":
            continue

        class_name = element.attrib.get("classname", "").strip()
        case_name = element.attrib.get("name", "unnamed test").strip()
        name = f"{class_name}.{case_name}" if class_name else case_name
        status = "PASS"
        detail = ""
        for child in element:
            child_name = local_name(child.tag)
            if child_name in {"failure", "error"}:
                status = "FAIL"
                detail = detail_from_element(child)
                break
            if child_name in {"skipped", "disabled"}:
                status = "SKIP"
                detail = detail_from_element(child)
        cases.append(TestCase(name, status, parse_duration(element.attrib.get("time")), detail))

    if not cases:
        return TestSummary([], "CTest JUnit XML contains no individual test cases.")
    return TestSummary(cases)


def integer(value: Any) -> int | None:
    if isinstance(value, bool):
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, float) and value.is_integer():
        return int(value)
    if isinstance(value, str):
        try:
            return int(value)
        except ValueError:
            return None
    return None


def first_integer(record: Mapping[str, Any], keys: tuple[str, ...]) -> int | None:
    for key in keys:
        if key in record:
            value = integer(record[key])
            if value is not None:
                return value
    return None


def metric_from_record(record: Mapping[str, Any], metric_name: str) -> Metric:
    aliases = METRIC_ALIASES[metric_name]
    for alias in aliases:
        nested = record.get(alias)
        if isinstance(nested, Mapping):
            covered = first_integer(nested, ("covered", "covered_count"))
            total = first_integer(nested, ("count", "total", "total_count"))
            if covered is not None and total is not None:
                return Metric(covered, total, True)

    covered_keys = tuple(f"{alias}_covered" for alias in aliases)
    total_keys = tuple(f"{alias}_{suffix}" for alias in aliases for suffix in ("total", "count"))
    covered = first_integer(record, covered_keys)
    total = first_integer(record, total_keys)
    if covered is not None and total is not None:
        return Metric(covered, total, True)
    return Metric()


def metrics_from_summary(summary: Mapping[str, Any]) -> dict[str, Metric]:
    files = summary.get("files", [])
    file_records = [item for item in files if isinstance(item, Mapping)] if isinstance(files, list) else []
    metrics: dict[str, Metric] = {}
    for name in METRIC_NAMES:
        root_metric = metric_from_record(summary, name)
        if root_metric.available:
            metrics[name] = root_metric
            continue
        aggregate = Metric()
        for record in file_records:
            aggregate.add(metric_from_record(record, name))
        metrics[name] = aggregate
    return metrics


def normalize_source_path(filename: str, source_root: Path) -> str:
    normalized = filename.replace("\\", "/").lstrip("./")
    root = str(source_root.resolve()).replace("\\", "/").rstrip("/")
    if normalized.startswith(root + "/"):
        return normalized[len(root) + 1 :]
    marker = "avs_core/"
    marker_offset = normalized.find(marker)
    if marker_offset >= 0:
        return normalized[marker_offset:]
    return normalized


def directory_coverage(summary: Mapping[str, Any], source_root: Path) -> list[tuple[str, int, dict[str, Metric]]]:
    groups: dict[str, tuple[int, dict[str, Metric]]] = {}
    files = summary.get("files", [])
    if not isinstance(files, list):
        return []

    for record in files:
        if not isinstance(record, Mapping):
            continue
        filename = record.get("filename")
        if not isinstance(filename, str):
            continue
        relative_path = normalize_source_path(filename, source_root)
        parts = [part for part in relative_path.split("/") if part]
        if len(parts) >= 2 and parts[0] == "avs_core":
            group = "/".join(parts[:2])
        elif parts:
            group = parts[0]
        else:
            group = "source root"

        if group not in groups:
            groups[group] = (0, {name: Metric() for name in METRIC_NAMES})
        count, metrics = groups[group]
        for name in METRIC_NAMES:
            metrics[name].add(metric_from_record(record, name))
        groups[group] = (count + 1, metrics)

    return [(group, count, metrics) for group, (count, metrics) in sorted(groups.items())]


def percent(metric: Metric) -> str:
    if not metric.available or metric.total == 0:
        return "n/a"
    return f"{metric.covered * 100 / metric.total:.1f}%"


def metric_detail(metric: Metric) -> str:
    if not metric.available:
        return "No reported data"
    return f"{metric.covered:,} / {metric.total:,} covered"


def format_duration(value: float | None) -> str:
    if value is None:
        return "n/a"
    if value >= 1:
        return f"{value:.2f} s"
    return f"{value * 1000:.0f} ms"


def navigation(active: str) -> str:
    items = (("overview", "index.html", "Overview"), ("tests", "tests.html", "Tests"), ("coverage", "coverage/index.html", "Coverage"))
    links = []
    for name, href, label in items:
        class_name = "active" if name == active else ""
        links.append(f'<a class="{class_name}" href="{href}">{label}</a>')
    return "".join(links)


def page(title: str, active: str, content: str) -> str:
    return f"""<!doctype html>
<html lang="en">
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{escape(title)}</title>
  <style>{BASE_CSS}</style>
</head>
<body>
  <header class="site-header">
    <div class="frame">
      <a class="brand" href="index.html">AviSynthPlus-UT</a>
      <nav aria-label="Report navigation">{navigation(active)}</nav>
    </div>
  </header>
  <main class="frame">
{content}
  </main>
</body>
</html>
"""


def metric_card(label: str, metric: Metric, class_name: str = "") -> str:
    return f"""      <section class="metric-card {class_name}">
        <div class="metric-label">{escape(label)}</div>
        <div class="metric-value">{percent(metric)}</div>
        <div class="metric-detail">{metric_detail(metric)}</div>
      </section>"""


def count_card(label: str, value: int, class_name: str = "") -> str:
    return f"""      <section class="metric-card {class_name}">
        <div class="metric-label">{escape(label)}</div>
        <div class="metric-value">{value:,}</div>
      </section>"""


def coverage_cell(metric: Metric) -> str:
    if not metric.available:
        return '<td class="coverage-cell">n/a</td>'
    return f'<td class="coverage-cell">{percent(metric)}<span>{metric.covered:,} / {metric.total:,}</span></td>'


def metadata_block(args: argparse.Namespace, generated_at: str) -> str:
    return f"""    <dl class="metadata">
      <div><dt>Test repository SHA</dt><dd><code>{escape(args.repository_sha)}</code></dd></div>
      <div><dt>AviSynthPlus submodule SHA</dt><dd><code>{escape(args.upstream_sha)}</code></dd></div>
      <div><dt>Generated</dt><dd>{escape(generated_at)}</dd></div>
      <div><dt>Run</dt><dd><a href="{escape(args.run_url, quote=True)}">GitHub Actions</a></dd></div>
    </dl>"""


def overview_page(
    args: argparse.Namespace,
    tests: TestSummary,
    coverage: dict[str, Metric],
    directories: list[tuple[str, int, dict[str, Metric]]],
    generated_at: str,
) -> str:
    status_class = "pass" if args.test_status == "PASS" else "fail"
    status_copy = "All CTest cases completed successfully." if args.test_status == "PASS" else "CTest reported a failure; coverage can be partial."
    table_rows = "".join(
        "<tr>"
        f"<td><code>{escape(group)}</code></td>"
        f"<td>{file_count:,}</td>"
        f"{coverage_cell(metrics['line'])}"
        f"{coverage_cell(metrics['branch'])}"
        f"{coverage_cell(metrics['function'])}"
        "</tr>"
        for group, file_count, metrics in directories
    )
    directory_table = (
        f"""      <div class="table-wrap">
        <table>
          <thead><tr><th>Source area</th><th>Files</th><th>Line</th><th>Branch</th><th>Function</th></tr></thead>
          <tbody>{table_rows}</tbody>
        </table>
      </div>"""
        if table_rows
        else '<p class="empty">gcovr did not report any AvsCore source files.</p>'
    )
    return page(
        "AviSynthPlus-UT Verification",
        "overview",
        f"""    <p class="eyebrow">Latest master result</p>
    <h1>Verification</h1>
    <p class="subtitle">Linux GCC test results and source coverage for the pinned AviSynthPlus revision.</p>

    <section class="status-panel {status_class}">
      <div>
        <p class="status-title">TESTS {escape(args.test_status)}</p>
        <p class="status-copy">{escape(status_copy)}</p>
      </div>
      <a class="button-link" href="{escape(args.run_url, quote=True)}">Open Actions run</a>
    </section>

    <div class="metric-grid">
{count_card('Passed tests', tests.passed, 'pass')}
{count_card('Failed tests', tests.failed, 'fail' if tests.failed else '')}
{count_card('Skipped tests', tests.skipped)}
{metric_card('Line coverage', coverage['line'])}
{metric_card('Branch coverage', coverage['branch'])}
{metric_card('Function coverage', coverage['function'])}
    </div>

    <section class="section">
      <h2>Source coverage</h2>
      <p class="scope-note">Scope: all AvsCore sources compiled by Linux GCC. Uncompiled platform-specific sources are not represented. Source paths are relative to the AviSynthPlus root and begin at <code>avs_core/</code>.</p>
{directory_table}
      <p><a class="button-link" href="coverage/index.html">Open detailed coverage</a></p>
    </section>

    <section class="section">
      <h2>Run details</h2>
{metadata_block(args, generated_at)}
    </section>

    <p class="footer">This site contains only the latest completed reporting run.</p>""",
    )


def tests_page(args: argparse.Namespace, tests: TestSummary, generated_at: str) -> str:
    failure_details = ""
    failures = [case for case in tests.cases if case.status == "FAIL"]
    if failures:
        failure_details = "".join(
            f"""      <details open>
        <summary>{escape(case.name)}</summary>
        <pre>{escape(case.detail or 'CTest did not include failure output.')}</pre>
      </details>"""
            for case in failures
        )
    elif tests.unavailable_reason:
        failure_details = f'<p class="empty">{escape(tests.unavailable_reason)}</p>'
    else:
        failure_details = '<p class="empty">No failing CTest cases were reported.</p>'

    order = {"FAIL": 0, "SKIP": 1, "PASS": 2}
    test_rows = "".join(
        "<tr>"
        f"<td><code>{escape(case.name)}</code></td>"
        f"<td class=\"status-{case.status.lower()}\">{escape(case.status)}</td>"
        f"<td>{format_duration(case.duration)}</td>"
        "</tr>"
        for case in sorted(tests.cases, key=lambda case: (order[case.status], case.name))
    )
    result_table = (
        f"""      <div class="table-wrap">
        <table>
          <thead><tr><th>CTest case</th><th>Result</th><th>Duration</th></tr></thead>
          <tbody>{test_rows}</tbody>
        </table>
      </div>"""
        if test_rows
        else f'<p class="empty">{escape(tests.unavailable_reason or "No CTest cases were reported.")}</p>'
    )
    return page(
        "AviSynthPlus-UT Test Results",
        "tests",
        f"""    <p class="eyebrow">Latest master result</p>
    <h1>Tests</h1>
    <p class="subtitle">CTest result details from the same Linux GCC coverage run.</p>

    <div class="metric-grid">
{count_card('Total tests', tests.total)}
{count_card('Passed tests', tests.passed, 'pass')}
{count_card('Failed tests', tests.failed, 'fail' if tests.failed else '')}
{count_card('Skipped tests', tests.skipped)}
    </div>

    <section class="section">
      <h2>Failure output</h2>
{failure_details}
    </section>

    <section class="section">
      <h2>All CTest cases</h2>
{result_table}
    </section>

    <section class="section">
      <h2>Run details</h2>
{metadata_block(args, generated_at)}
    </section>""",
    )


def main() -> int:
    args = parse_args()
    with args.coverage_summary.open(encoding="utf-8") as stream:
        summary = json.load(stream)
    if not isinstance(summary, Mapping):
        raise ValueError("gcovr JSON summary must contain an object")

    tests = parse_junit(args.junit)
    coverage = metrics_from_summary(summary)
    directories = directory_coverage(summary, args.source_root)
    generated_at = datetime.now(timezone.utc).strftime("%Y-%m-%d %H:%M UTC")

    args.output_dir.mkdir(parents=True, exist_ok=True)
    data_dir = args.output_dir / "data"
    data_dir.mkdir(parents=True, exist_ok=True)
    (args.output_dir / "index.html").write_text(
        overview_page(args, tests, coverage, directories, generated_at), encoding="utf-8"
    )
    (args.output_dir / "tests.html").write_text(
        tests_page(args, tests, generated_at), encoding="utf-8"
    )
    dashboard_data = {
        "test_status": args.test_status,
        "tests": {
            "total": tests.total,
            "passed": tests.passed,
            "failed": tests.failed,
            "skipped": tests.skipped,
            "unavailable_reason": tests.unavailable_reason,
        },
        "coverage": {
            name: {
                "covered": coverage[name].covered,
                "total": coverage[name].total,
                "available": coverage[name].available,
            }
            for name in METRIC_NAMES
        },
        "repository_sha": args.repository_sha,
        "upstream_sha": args.upstream_sha,
        "run_url": args.run_url,
        "generated_at": generated_at,
    }
    (data_dir / "dashboard.json").write_text(
        json.dumps(dashboard_data, indent=2, sort_keys=True) + "\n", encoding="utf-8"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
