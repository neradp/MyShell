# MyShell

A customizable Windows context menu based on
[Nilesoft Shell](https://github.com/moudey/Shell), with third-party file-manager
integration and an optional Windows 11-style file-action toolbar.

## Features

- Customize menu items, submenus, icons, and themes using NSS configuration.
- Capture file selections in applications that use Windows Shell context menus,
  including Total Commander, Everything, and EverythingToolbar.
- Retain the original Explorer and taskbar integration, including compatibility
  with StartAllBack.
- Add an optional horizontal toolbar with **Cut, Copy, Paste, Rename, Delete**.
- Keep toolbar buttons compact and edge-aligned, with labels and separators.
- Use the host's original file commands, clipboard behavior, and confirmation
  dialogs.

Third-party integration depends on the host application, its menu implementation,
and the active configuration. Native tests do not replace interactive testing
across Windows versions and applications.

## Download and installation

1. Open [GitHub Releases](https://github.com/neradp/MyShell/releases).
2. Select a release and download **MyShell-vVERSION-x64.zip** from **Assets**
   (not the automatically generated source archives).
3. Extract the package to a permanent installation directory.
4. Use the included `shell.exe` manager to register `shell.dll`. Registration
   may require administrator privileges.
5. Restart Explorer and any applications that already loaded the previous DLL.

The package contains:

- `shell.dll`: the context-menu extension.
- `shell.exe`: the registration and management application.
- `shell.nss` and `imports`: the starter configuration and its dependencies.
- `LICENSE`: the license.

Initial releases are marked **Pre-release** while compatibility testing continues.

Back up your existing configuration before updating. Keep the complete import
tree when using a custom NSS configuration; replacing only `shell.nss` may leave
required imports missing.

A loaded DLL may be locked by Explorer or other applications. Close the affected
applications before replacing it, or restart Windows after updating.

## Configuration

Menu customization is defined in `shell.nss` and its imported NSS files.
If a custom configuration is already configured, edit that active configuration
rather than the starter file.

### Allow third-party applications

The starter configuration restricts customization to Explorer:

```nss
exclude.where = !process.is_explorer
```

Remove or adjust this condition in the `settings` block to allow other hosts.
For example, to allow Explorer, Total Commander, and Everything:

```nss
exclude.where = !process.is_explorer and
                !str.equals(process.name, ['TOTALCMD64', 'Everything'])
```

Exclusion controls menu customization; it does not prevent Windows from loading
the registered DLL into applications that use Shell components.

### Enable the toolbar

Add this inside the existing `settings` block:

```nss
toolbar.enabled = true
```

The toolbar is disabled by default. It appears in suitable file, folder, drive,
and background menus when the host provides a recognized file action. It is
not added to taskbar, Start, or edit-control menus.

- Icons use NSS image definitions, theme colors, and `theme.image.size`.
- Buttons have a fixed width and stay aligned to the menu's leading edge as
  other items make the menu wider. Labels appear under each icon, with vertical
  separators between buttons and a horizontal divider below the toolbar. Labels
  have room around the separators and wrap when needed; the row grows to fit
  their text. The divider keeps the theme's separator margin below it, and the
  button highlight stays clear of the vertical separators.
- Moving the mouse highlights the enabled button under the pointer.
- Hovering over a recognized action displays its localized title.
- Use Up/Down to select the toolbar row, Left/Right to choose a button, and
  Enter to invoke it.
- Missing or disabled host actions are greyed out.
- Matching native text commands are hidden while the toolbar is present.
  Custom commands and submenus are preserved.
- Set `toolbar.enabled = false` to restore the normal text-command behavior.

Action recognition uses Shell's localized Windows menu-title mapping, not
hard-coded Slovak or English labels. Arbitrary host labels may not be recognized.
NSS rules that remove native actions or relocate them into custom submenus can
also prevent the toolbar from finding them. Keep the original actions in place
when using the toolbar; its duplicate filtering hides their text entries.

The icon row does not expose separate accessible button objects. For
screen-reader workflows, disable the toolbar and use the normal text commands.

## Build

The [Build workflow](.github/workflows/build.yml) runs on a Windows GitHub runner.
It restores native NuGet dependencies, builds the **release x64** DLL and manager,
runs native tests, and validates the portable package.

Builds run on pushes and pull requests to `main`, version tags matching `v*`,
or can be started manually from GitHub Actions. No local development tools are
required to use this workflow.

### Publishing a release

Push a new version tag pointing to the source revision to release:

```powershell
git tag v0.1.0
git push origin v0.1.0
```

Use a new version for each subsequent release. After the tagged build and tests
succeed, the release job packages the tested build as
`MyShell-v0.1.0-x64.zip` and publishes it to GitHub Releases as a pre-release.
Failed builds do not publish a release. Only the release job has repository
write permissions; ordinary builds retain read-only permissions.

For a local build, install Visual Studio C++ tools with the **v143 toolset**,
a Windows SDK, and NuGet. Run from a developer PowerShell:

```powershell
nuget restore .\src\Shell.sln -NonInteractive
msbuild .\src\Shell.sln /m '/t:dll;exe' /p:Configuration=release /p:Platform=x64
msbuild .\src\tests\SelectionTests.vcxproj /p:Configuration=release /p:Platform=x64
& .\src\tests\bin\selection-tests.exe
```

Product binaries are written to `src\bin`. The workflow builds the portable
package, not an MSI installer.

## Source layout

- [src/Shell.sln](src/Shell.sln): product solution.
- [src/dll](src/dll): menu rendering, selection capture, parser, and hooks.
- [src/exe](src/exe): registration and management application.
- [src/shared](src/shared): shared code and resources.
- [src/bin](src/bin): runtime configuration and product output directory.
- [src/tests](src/tests): native selection and toolbar tests.
- [.github/workflows/build.yml](.github/workflows/build.yml): x64 CI build.

## Troubleshooting

- **Changes do not appear:** verify the active NSS configuration and restart
  applications that loaded an older DLL.
- **A third-party menu remains unchanged:** check `exclude.where` and confirm
  the application uses Windows Shell context menus.
- **Toolbar buttons are disabled:** check whether the host provides enabled
  actions and whether NSS rules removed or relocated the original commands.
- **Configuration fails to load:** check imports and `shell.log` next to the
  registered DLL.

## License and attribution

MyShell is based on Nilesoft Shell. See [LICENSE](LICENSE) for license terms.
Third-party selection support includes an adapted implementation inspired by
[moudey/Shell#797](https://github.com/moudey/Shell/pull/797).
