# CI validation and reviewed alpha publication

The default CircleCI workflow is validation-only. It builds every platform in
`.circleci/config.yml`, retains each package, metadata file and checksum as a
CircleCI artifact, and never requests a Cloudsmith credential.

The Debian 12 x86_64 job also runs the plugin tests and the standalone eclipse
engine tests. It downloads `de440s.bsp` from the project's external data host
and accepts it only when this SHA-256 matches:

```
c1c7feeab882263fc493a9d5a5b2ddd71b54826cdf65d8d17a76126b260a49f2
```

## Reviewed alpha publication

Publication must be started explicitly with the CircleCI pipeline parameter
`run_workflow_deploy=true`. This performs fresh builds of the complete matrix
and then stops at `hold-for-alpha-approval`. Only after a maintainer approves
that gate does the final job receive the `celestial-navigation-deployment`
context and publish the retained packages.

That restricted context must define:

- `CLOUDSMITH_API_KEY`
- `CELESTIAL_CLOUDSMITH_REPO`, for example
  `pob220/celestial-navigation-alpha`

The repository variable is deliberately required rather than defaulted to an
official OpenCPN repository. This prevents an accidental official publication
while the contribution is under review. Rick/OpenCPN can point the same
reviewed workflow at the official alpha repository when they are ready.
