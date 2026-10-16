# Publish the corrected Celestial Navigation packages

These packaging changes do not change navigation calculations or bump the
plugin version. Fresh publication uses a new Cloudsmith build number and new
download URLs; there is no need to overwrite packages or move existing tags.

1. Merge this packaging fix into `rgleason/celestial_navigation_pi:master`.
   The version in `CMakeLists.txt` is the release that will be built. If a new
   patch version is desired, change it before starting the new pipeline.
2. Confirm the CircleCI project has `CELESTIAL_CLOUDSMITH_REPO` set to
   `opencpn/celestial-navigation-prod`, and its `CLOUDSMITH_API_KEY` can write
   there. Do not put the secret key in Git, a PR, or pipeline parameters.
3. Trigger a **new pipeline on master** with:

   ```text
   validate_windows_x64_only = false
   run_workflow_deploy = true
   ```

   Do not use the WeatherRouting parameter names here. Do not rerun an old
   pipeline: it checks out the old source and can regenerate the same defects.
4. Wait for every platform build and `prepare-publication` to pass.
   This new preparation job creates all final tarballs and XML before the
   approval hold. It checks the complete catalogue schema, matching embedded
   and external metadata, retained build hashes and source commit.
5. Review that job's `reviewed-publication` artifacts. In `publication.json`,
   confirm the intended source commit and repository
   `opencpn/celestial-navigation-prod`. Test importing its Windows x64 tarball
   into 64-bit OpenCPN before approving publication.
6. Approve `approve-publication` in **that same pipeline**, then wait for
   `publish-reviewed` to pass. It uploads the reviewed files after checking all
   hashes again. It does not overwrite existing Cloudsmith package versions.
7. Download the newly published Cloudsmith XML and tarballs and confirm they
   match the reviewed files. Replace the superseded Celestial Navigation
   catalogue entries using those XML files, unchanged, and wait for catalogue
   validation. Do not copy the old failed PR's XML or substitute URLs manually.

For Windows x64, `target=msvc-64`, `build-target=msvc`, and
`target-arch=x86_64`. Source-code links now identify the actual build repository
and full commit, including Android and Windows. `<source>` is a source-code
link; `<tarball-url>` is the downloadable plugin archive.
