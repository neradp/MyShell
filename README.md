# MyShell

An analysis-led continuation of [Nilesoft Shell](https://github.com/moudey/Shell)
for Windows 11 context menus, preserving existing NSS configurations and
compatibility with StartAllBack.

**Status: third-party selection working in user testing, November 6, 2026.**
An adapted implementation of PR #797, native selection tests, and an x64
GitHub Actions workflow are present. The DLL, manager, and selection tests have
built successfully. The user reports working third-party menus after the
handler-lifetime correction. The optional icon toolbar described below is new
and still requires a GitHub build and interactive validation.
ReShell has been inspected statically. A Ghidra decompilation attempt was stopped
at the user's request; no completed pseudocode comparison is available.
Static findings must not be confused with runtime compatibility tests.

## Optional file-action toolbar

Add this inside the `settings` block of your active NSS configuration:

```nss
toolbar.enabled = true
```

The default is disabled, preserving existing menus. The toolbar appears at the
top of file/folder/drive selection menus and background menus when the host
provides at least one recognized file action. It is never added to taskbar,
Start, or edit-control menus.

Buttons, in order: **Cut, Copy, Paste, Rename, Delete**. Missing or disabled
native actions are greyed out. Recognition uses Shell's existing localized
command identifiers, not fixed numeric command IDs or English title matching.
Buttons dispatch the original host command, retaining its clipboard behavior,
confirmation dialogs, and selection semantics. The toolbar takes its actions
from the original host menu independently of NSS rules that move or remove
text entries. Original text entries are not removed by the toolbar itself.

Hover over an available host action for its localized title. Use Up/Down to
select the toolbar row, Left/Right to choose an enabled button, and Enter to
invoke it. Existing text commands remain the accessible alternative for
screen readers and normal menu shortcuts. Button geometry scales with DPI
and reverses in right-to-left menus.

GitHub runs toolbar geometry, disabled-command, native-ID, and keyboard
navigation tests alongside selection tests. Interactive rendering, mouse
dispatch, and host behavior still require testing with the resulting DLL.

## Recommendation

Use the original Shell sources now located in [src](src/) as the baseline. Treat ReShell as
a behavioral reference, not a wholesale replacement of working behavior.
Proceed without ReShell decompilation. Evaluate the available PR #797 sources
against the original behavior before releasing the adapted implementation.

| Requirement | Assessment |
|---|---|
| Build without local Visual Studio | Feasible through GitHub Actions Windows runners. |
| Preserve the original NSS configuration | A primary goal; requires tests with the complete original import tree. |
| Working taskbar menus with StartAllBack | Required; the reported ReShell regression has not been reproduced or explained conclusively. |
| Integrate PR #797 | Feasible after review and adaptation; not a demonstrated taskbar fix. |
| Alternative-file-manager support | Evaluate PR #797 and host behavior without depending on ReShell internals. |
| Windows 11-style icon toolbar inside the classic menu | Feasible development target; requires C++ rendering, input, and accessibility work. |
| Embed the actual native modern toolbar in a classic menu | No documented public API for this integration. |
| Recover original ReShell sources from its binaries | Not feasible; decompilation produces approximate pseudocode, not the original project. |
| Guarantee compatibility with all future Windows builds | Not feasible; the baseline relies on hooks and Windows implementation details. |

## 1. Available material

- [src](src/) contains C++ sources, the NSS parser, menu rendering,
  selection handling, and taskbar hooks.
- [ReShell-Releases-main](ReShell-Releases-main/) contains release `0.1.0-alpha`,
  binaries, configuration, and documentation, but no sources for its changes.
- The user's working configuration is at `C:\tools\etc\shell`. It was inspected
  without modification or publication.
- Local Shell sources already contain Insider-channel/theme changes described in
  [Issue700-Fix.md](Shell-main/Issue700-Fix.md). Do not assume this archive is an
  untouched upstream checkout. Its historical testing claims were not independently
  verified during this analysis.
- The workspace is not a Git repository. The exact upstream revision of the local
  source archive is not established.

### Repository layout

The project has been moved into the workspace root, ready to become the MyShell
Git repository. Upload or commit this root, not the remaining reference folder.

- [src/Shell.sln](src/Shell.sln): product solution.
- [.github/workflows/build.yml](.github/workflows/build.yml): portable builds and x64 selection tests.
- [src/tests](src/tests/): native selection and toolbar tests.
- [LICENSE](LICENSE): original license.
- [.gitignore](.gitignore): excludes the remaining `Shell-main` folder,
  `ReShell-Releases-main`, local `analysis`, build outputs, and IDE files.

Upstream documentation, screenshots, templates, old distribution packages, and
historical issue/PR Markdown files remain in the ignored `Shell-main` folder,
not in the active project. Build resources, bundled libraries, and runtime NSS
configuration remain under `src` because they are required for building or running
Shell. Stale upstream submodule metadata was not activated at root;
the required native libraries are already included under
[src/shared/Library](src/shared/Library/).

## 2. Taskbar regression and configuration compatibility

The user reports that the original Shell worked with StartAllBack, while ReShell
does not provide a working context menu. No live Explorer comparison has been run.

### Verified configuration differences

The original main configuration uses:

```nss
$sys_object_taskbar = wnd.is_taskbar and wnd.parent.name==''
```

Its taskbar import is conditional on this expression. The imported configuration
adds another condition:

```nss
menu(where=@(this.count == 0 && window.parent.name == null)
     type='taskbar' image=icon.settings expanded=true)
```

Window classification, parent-window properties, menu type, and existing-item
count must all match. A change in any of these can hide custom entries despite a
working hook. This is a hypothesis, not a confirmed explanation.

The local original parser supports both `wnd` and `window`, as shown in
[FuncExpression.cpp](src/dll/src/Expression/FuncExpression.cpp).
ReShell compatibility needs separate verification. Absence of a literal identifier
in binary strings or documentation does not establish that it was removed.

The supplied ReShell configuration:

- uses language imports and `loc.*` labels;
- includes `exclude.where = !process.is_explorer`, which the user's main
  configuration does not;
- supplies a different taskbar import;
- lacks several user-specific imports, including `all.power.nss`,
  `nss.winver.nss`, `recycle.bin.nss`, and `spotlight.nss`.

**Copying only the main NSS file is insufficient.** Missing imports can prevent
configuration loading. Differences between `title.*` and `loc.*` may require
migration, but the exact identifiers removed by ReShell have not been established.
All common imports compared with the user's configuration differed in content.

The Explorer-only exclusion does not itself explain a taskbar problem inside
`explorer.exe`, but it matters for alternative file managers.

### Relevant baseline code

In [Main.cpp](src/dll/src/Main.cpp):

- `taskbar_t::is_allowed_area` identifies click targets using UI Automation;
- `TaskbarProc` and `TaskbarSubclassProc` intercept taskbar messages;
- `ShowTaskbarContextMenu` creates the popup;
- `NtUserTrackPopupMenu` invokes the modified menu and command dispatcher.

[ContextMenu.cpp](src/dll/src/ContextMenu.cpp) treats custom commands
and original system command IDs differently. Preserve owner-window identity,
command-ID return behavior, `TPM_RETURNCMD` / `TPM_NONOTIFY` semantics, and the
original host dispatch chain.

Both Shell and StartAllBack affect Explorer. A hook/subclass conflict is possible,
but it has not been demonstrated.

### Diagnostic comparison

1. Record Windows, StartAllBack, and original Shell versions.
2. Preserve the full working configuration and every imported file.
3. Compare original Shell and ReShell using that same configuration.
4. Separately test ReShell's supplied configuration.
5. Check the active configuration path and the first error in `shell.log`.
6. Distinguish: no popup, missing custom entries, or entries that do not execute.
7. In an isolated test configuration, try an unconditional harmless taskbar command
   such as opening Calculator, then restore conditions incrementally.

Verify which DLL is actually loaded before drawing conclusions. Registration alone
does not guarantee that Explorer has released the previous DLL. Perform switching
only in an agreed test environment, not automatically in the active desktop.

## 3. PR #797 and alternative file managers

[PR #797](https://github.com/moudey/Shell/pull/797) was open and unmerged when
checked. Reviewed head: `42522acfff10a22473a7b90fbb8380e0b9e6b5f0`.

It retrieves selections through a helper COM context-menu handler when the
original ShellBrowser-based path is insufficient. It adds a class factory,
`ShellExtSelectionRetriever`, global selection state, and a placeholder menu item,
and changes menu initialization, selection retrieval, and COM DLL lifetime.
The patch affects ten files.

### Local adaptation

The local implementation follows the PR's helper COM-handler approach, but does
not apply its patch verbatim. Selection belongs to each handler and is associated
with the menu handle and calling thread. No placeholder command is inserted,
and no synthetic menu initialization is performed. The original Explorer and
taskbar selection paths remain the preferred paths; the fallback is restricted
to non-Explorer file contexts. Initialization and parsing failures are logged.

Tests cover COM output contracts, reference counts, background/file selections,
menu/thread isolation, one-shot consumption, and invalidated associations.
They are source-level deliverables awaiting the first Windows CI run; this does
not establish compatibility with StartAllBack or any alternative file manager.

The author reports successful tests in XYplorer, Total Commander, Double Commander,
and Everything, but unsuccessful tests in Directory Opus and OneCommander.
These author-reported results have not been reproduced here.

The PR does not implement an icon toolbar, modern Windows 11 menu customization,
or a verified StartAllBack fix. It does not establish equivalence with ReShell.

Before integration:

- Establish and test an unchanged baseline.
- Keep Explorer/taskbar handling intact; constrain selection fallbacks to relevant
  file contexts.
- Examine global-state isolation, nested menus, concurrency, and COM lifetimes.
- Test the changed timing of `WM_INITMENUPOPUP` and exclusion-test initialization
  for reentrancy and behavior changes.
- Ensure the placeholder cannot remain visible when hooks or initialization fail.
- Validate empty selections, background menus, virtual objects, mixed parents,
  error returns, and process architecture.
- Explicitly allow supported alternative processes in the configuration.

These are integration risks to investigate, not claims that every item is a
confirmed defect.

### Proceeding without ReShell decompilation

The user reports that ReShell supports alternative programs through a different
implementation. Its internal design and superiority remain unverified.
ReShell decompilation is no longer part of the implementation plan.

Use the original Shell and PR #797 sources to investigate selection ownership,
menu association, lifecycle, placeholder handling, and isolation from taskbar
behavior. Validate the chosen implementation in the actual target applications.
ReShell can optionally serve as a behavioral reference in an agreed test
environment; it is not a source dependency or a prerequisite for MyShell.

### Third-party runtime correction

The x64 build and selection tests passed in GitHub run
[42124363857](https://github.com/neradp/MyShell/actions/runs/42124363857).
The user initially reported native, unmodified menus in Everything and Total
Commander after a full PC restart. The installed DLL matches the artifact hash.
Read-only inspection of both host processes confirms that the
`NtUserTrackPopupMenuEx` import points into MyShell.

Runtime diagnostics now establish that Total Commander releases the zero-ID
helper immediately after `QueryContextMenu`, before the popup consumes its
selection. The handler now reserves one internal command ID without adding a
visible item, so the Shell aggregator can retain its command mapping. This
user reports working third-party menus after deploying this retention change.
This is user-reported runtime validation, not an automated host compatibility
suite. Diagnostics
now log helper queries, premature helper destruction, selection consumption,
window-classification rejection, and configuration exclusion. No file names or
selected paths are included. After rebuilding, reproduce one file-result menu
per host and inspect `shell.log` next to the registered DLL. In particular,
`Selection helper released before popup capture` identifies the lifetime
problem that the direct handler tests originally did not cover.

The user's active configuration has no Explorer-only exclusion. The bundled
starter configuration does, so other installations must deliberately allow
the desired host processes. EverythingToolbar uses native `IContextMenu` and
`TrackPopupMenuEx` in its published source, but actual compatibility has not
been verified. Do not treat passing unit tests as proof of host integration.

## 4. Windows 11-style toolbar in the classic menu

The implemented feature is a custom icon toolbar, not the native modern menu.
Commands: Cut, Copy, Paste, Rename, and Delete, with context-sensitive enabled
state. See [Optional file-action toolbar](#optional-file-action-toolbar) for
activation and limitations.

The existing `OnMeasureItem` / `OnDrawItem` implementation in
[ContextMenu.cpp](src/dll/src/ContextMenu.cpp) provides a rendering
starting point. A dedicated owner-drawn row now provides horizontal buttons.

A normal `HMENU` row has one ID and selected state. Five buttons require custom
hit-testing, focus, keyboard handling, tooltips, and accessibility. Drawing five
icons or using NSS `col` does not implement a usable toolbar.

Implementation constraints:

1. Make the toolbar optional and initially limit it to suitable file/folder contexts,
   excluding the taskbar.
2. Integrate geometry and input with the popup's lifetime; verify space reservation,
   dismissal, focus, and edge positioning.
3. Reuse original system command IDs and host dispatch where possible.
4. For a separate COM implementation, use the correct `IContextMenu` selection/site
   and `InvokeCommand`; arbitrary IDs or verbs are not sufficient.
5. Avoid simulated keystrokes, hard-coded Explorer IDs, `cmd /c del`, or replacing
   Shell clipboard formats with plain path strings.
6. Preserve an off switch and baseline menu behavior.

Paste must preserve copy/move clipboard semantics. Delete must respect the Recycle
Bin and host confirmation behavior. Rename must preserve the correct editing
context. Unsupported commands should be disabled or hidden, never silently replaced
by a different operation.

Validate DPI scaling, light/dark themes, high contrast, localized labels, narrow menus,
keyboard navigation, screen readers, and multiple monitors. A reliable effort
implementation requires interactive host validation as well as unit tests.

Microsoft documents modern menu extensions through `IExplorerCommand` and app
identity, including sparse packages for unpackaged Win32 applications. This does
not expose arbitrary control over the built-in toolbar or embed it in a classic
popup. Undocumented modern-menu hooking is a separate, riskier project.

## 5. ReShell binary analysis

Static inspection was performed without executing the binaries or loading the DLL:

| File | Size | Architecture | CLR/.NET | Signature |
|---|---:|---|---|---|
| `shell.dll` | 2,439,168 bytes | x64 (`0x8664`) | No | NotSigned |
| `shell.exe` | 420,864 bytes | x64 (`0x8664`) | No | NotSigned |

The DLL exports `DllCanUnloadNow` and `DllGetClassObject`, matching the names in
[Shell.def](src/dll/src/Shell.def). Imports include Win32, Shell/COM,
Direct2D, DirectWrite, and UxTheme APIs. Imports do not establish implementation
details or compatibility.

SHA-256 of the inspected release:

```text
shell.dll F72D2B5C5A9D5D9770F3AAF32BF2C7D12B65F55763041247456CF946AF72C0C0
shell.exe A1077CC82D7D425028CDB48BEBB7D049C3F1961086D77089186929FD149CE3C8
```

These are native binaries; .NET decompilers are inappropriate. Local Ghidra analysis
can produce disassembly, cross-references, and approximate C pseudocode. It cannot
recover original source names, comments, macros, project files, or a reliable
source-level diff. PE layout does not establish exact compiler flags.

Keep findings separate from hypotheses. Function names assigned during analysis are
analyst labels unless backed by symbols. Decompiled types and ownership require
verification against assembly and known API contracts.

Both supplied license files contain MIT terms. Preserve relevant notices when
reusing or distributing material, and review third-party dependency licenses.
License availability does not imply that modified sources are available.

## 6. Building through GitHub

Create a **MyShell** repository in this workspace root. The project has already
been moved here. Commit the tracked project files and respect [.gitignore](.gitignore);
do not upload the ignored original folders or private user configuration.
The solution is `src\Shell.sln`.

The project uses MSVC `v143`, Windows SDK, C++20, and NuGet VC-LTL 5.1.1.
Bundled Detours/PlutoSVG libraries for x64/x86/ARM64 are in
[src/shared/Library](src/shared/Library/). Ensure these and the complete
NSS/language import tree are tracked.

The active [build.yml](.github/workflows/build.yml) uses relative output paths,
builds DLL/manager targets without MSI, supports manual dispatch, and checks
package files. It builds only x64 and runs the native selection tests on x64.
The first GitHub build failed with a WRL namespace ambiguity and MSVC C2712
(allocation inside an SEH-protected export). These were corrected by qualifying
`Nilesoft::Windows` and moving factory allocation into a separate function.
The [second GitHub run](https://github.com/neradp/MyShell/actions/runs/42123270549)
compiled both `shell.dll` and `shell.exe`, and compiled the tests with zero errors
and warnings. Test execution failed because its `SHCreateDataObject` fixture
passed an absolute PIDL as a parent-relative child. The test now supplies the
parent PIDL and final child identifier, checks the selected object's identity
and parent, and reports HRESULT values. A local Windows API probe reproduced the
original `0x80040064` failure and verified successful array/item/parent retrieval
with the corrected fixture. The complete C++ tests and artifact packaging still
require a new CI run; GUI compatibility remains unverified.

### Proposed workflow

The following is a simplified x64-only example. The active workflow additionally
runs selection tests. Neither has been verified
by a successful CI run:

```yaml
name: MyShell build

on:
  workflow_dispatch:
  push:
    branches: [main]
  pull_request:
    branches: [main]

permissions:
  contents: read

jobs:
  build:
    runs-on: windows-2022
    steps:
      - uses: actions/checkout@v4
      - uses: microsoft/setup-msbuild@v2
      - uses: NuGet/setup-nuget@v2

      - name: Restore native packages
        shell: pwsh
        run: |
          nuget restore .\src\Shell.sln -NonInteractive
          if ($LASTEXITCODE -ne 0) { throw "NuGet restore failed" }

      - name: Build x64 DLL and manager
        shell: pwsh
        run: |
          msbuild .\src\Shell.sln /m '/t:dll;exe' /p:Configuration=release /p:Platform=x64
          if ($LASTEXITCODE -ne 0) { throw "MSBuild failed" }

      - name: Validate portable package
        shell: pwsh
        run: |
          foreach ($path in @(
            '.\src\bin\shell.dll',
            '.\src\bin\shell.exe',
            '.\src\bin\shell.nss',
            '.\src\bin\imports\taskbar.nss',
            '.\src\bin\imports\lang\en.nss'
          )) {
            if (!(Test-Path -LiteralPath $path -PathType Leaf)) {
              throw "Missing package file: $path"
            }
          }
          Copy-Item -LiteralPath .\LICENSE -Destination .\src\bin\LICENSE

      - uses: actions/upload-artifact@v4
        with:
          name: MyShell-x64
          if-no-files-found: error
          path: |
            src/bin/**
            !src/bin/*.lib
            !src/bin/*.exp
            !src/bin/*.pdb
            !src/bin/*.pgd
```

Download the artifact after a successful Actions run and test it in an agreed Windows
environment. CI compilation and file checks do not establish GUI behavior,
completeness of dynamic imports, or StartAllBack compatibility. Runner availability
and billing depend on the repository/account.

Keep internal `shell.exe`, `shell.dll`, `shell.nss`, CLSIDs, and registry keys intact
initially. Renaming them is an integration task, not just changing `TargetName`.
Without a separate registration identity, MyShell is an alternative installation
of Shell, not a safely side-by-side product.

## 7. Implementation sequence and acceptance tests

1. Compile the adapted implementation and run the native tests through GitHub.
2. Compare with the original Shell using the complete working configuration and
   verify StartAllBack taskbar behavior.
3. Test required alternative file managers and correct any selection regressions.
4. Add an optional toolbar prototype, initially in Explorer and outside the taskbar.
5. Package licenses, configuration, diagnostics, and rollback instructions.

| Area | Acceptance criteria |
|---|---|
| Taskbar | Popup opens; custom and original commands execute exactly once. |
| StartAllBack | Existing entries/configuration remain available; clicks are not swallowed. |
| Multiple monitors | Primary/secondary taskbars and differing DPI behave correctly. |
| NSS | Original imports/aliases work; syntax errors provide diagnostics. |
| Explorer | Files, folders, background, desktop, Recycle Bin, and virtual objects work. |
| Selection | Single/multiple/empty selections, mixed parents, UNC, and Unicode paths. |
| Toolbar operations | Correct targets/states, clipboard semantics, confirmations, and Recycle Bin; disposable test files only. |
| Input | Mouse, keyboard, Escape, focus, tooltips, and accessibility. |
| Appearance | Light/dark, high contrast, DPI, and screen-edge placement. |
| Regression | Toolbar off matches baseline; no crashes, freezes, or duplicate commands. |

## References

- [Nilesoft Shell](https://github.com/moudey/Shell)
- [PR #797](https://github.com/moudey/Shell/pull/797)
- [ReShell README](ReShell-Releases-main/README.md)
- [Shell license](LICENSE)
- [ReShell license](ReShell-Releases-main/license.txt)
- [Microsoft: Windows 11 context-menu extensions](https://blogs.windows.com/windowsdeveloper/2021/07/19/extending-the-context-menu-and-share-dialog-in-windows-11/)
- [Microsoft: IExplorerCommand](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nn-shobjidl_core-iexplorercommand)
- [Microsoft: IContextMenu::InvokeCommand](https://learn.microsoft.com/en-us/windows/win32/api/shobjidl_core/nf-shobjidl_core-icontextmenu-invokecommand)
