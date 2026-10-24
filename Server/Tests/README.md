# Component tests

`component-tests` loads the built component binaries and drives them through the SDK the way the server does: each component goes through `onLoad`/`onInit`/`onReady` against a mocked core, gets player connect, update and click events through the mocked player pool, and sends its RPCs to a mocked network that records them.

The tests only use the SDK, never component internals. That is what makes them useful for the Rust port: the same tests run against a build from before a component was ported, so they show the port didn't change behaviour.

## Running

Configure with `-DBUILD_COMPONENT_TESTS=ON` (Python 3 is needed to generate the mocks), build, then run from the build directory:

```bash
ctest -C RelWithDebInfo --output-on-failure
```

Or run the executable directly. It takes an optional test name filter:

```bash
build/Server/Tests/RelWithDebInfo/component-tests gangzones
```

## Comparing against a baseline

To check a port against the C++ it replaced, build the components at the commit before the port (same compiler and architecture), then point the tests at that build's components:

```bash
component-tests --baseline path/to/old/build/Output/RelWithDebInfo/Server/components
```

Checks written with `CHECK_PORTED` cover behaviour a port changed on purpose (for example undefined behaviour that is now handled) and are skipped in baseline runs. Everything else must pass on both.

The last pure C++ versions of the components ported so far are at commit `72bf89ce`.

## Writing tests

Add a `TEST_CASE` in the component's file, using `ComponentFixture<IYourComponent>` to load it and `MockPlayer`/`f.core.players` to connect players and send them events. When porting a new component, write its tests against the existing C++ first, then check the port against them.

The mocks implement only what components have needed so far; every other virtual is a stub returning a default value, generated from the SDK headers by `genmock.py`. If a component needs more of the core, implement those methods in `mocks.hpp` and add them to `SKIP` in `genmock.py`.
