# Engine patches

Fixes for the pinned `evogine` revision. CMake applies each `*.patch` to the
submodule at configure time (skipping ones already applied), so a fresh clone
builds without manual steps. Remove a patch once the fix lands upstream and the
submodule is bumped.

- `evogine-dangling-window-reference.patch` - `primitive::Dictionary::Get`
  returned by value, so `window::Manager::GetWindow` returned a reference to a
  temporary. `RenderPipeline2D::Setup(true)` (the ImGui path) read freed stack
  memory and crashed at startup.
