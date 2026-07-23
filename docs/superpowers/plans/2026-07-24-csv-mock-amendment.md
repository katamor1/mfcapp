# CSV Mock and Provisional ID Plan Amendment

> This amendment is authoritative where it conflicts with `2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`.

**Decision source:** Product owner instruction on 2026-07-24.

## Changed assumptions

- Vendor COM IDs will be assigned later.
- Development uses provisional IDs numbered sequentially from 1.
- Mock machine responses come from CSV.
- CSV and the future COM adapter are selected behind `IMachineStateReader` and `IMachineCommandGateway`.
- The earlier restriction that only `dataId=12` may be registered is superseded for the mock environment. It remains true that no provisional number is a production vendor contract.

## Implementation changes

### Completed now

- Add `ProvisionalDataIds.h` as the only numeric ID catalog.
- Add `CsvScenarioLoader`, parsing `at_ms,data_id,sub_id1,sub_id2,value`.
- Add `config/mock/machine-responses.csv`.
- Convert CSV responses into `FakeScenario` and continue using `FakeMachineGateway` through the existing Application ports.
- Validate sequential IDs, time-based override behavior, CSV quoting, duplicate addresses, and missing required responses.

### Task 9 amendment

The composition root shall:

1. Use `config/mock/machine-responses.csv` as the default development response source.
2. Accept `--mock-csv=<path>` to select another scenario.
3. Load the file with `CsvScenarioLoader::Load`.
4. Construct `FakeMachineGateway` from the resulting `FakeScenario`.
5. Display a clear startup error and disable operations if the CSV cannot be opened or validated.
6. Keep `--fake` as an optional compatibility alias, but do not construct `FakeScenario::StandardDemo()` in normal application startup.

### Task 13 amendment

- Introduce the real COM adapter as another implementation of the same Application ports.
- Move vendor-assigned IDs into a production catalog when supplied.
- Keep the provisional catalog for CSV fixtures and tests only.
- Add adapter contract tests proving that logical fields map to the vendor IDs; do not change Domain, Application, Presenter, or View code.

## Acceptance additions

- All provisional IDs used by the CSV fixture are declared once and are consecutive from 1.
- No Domain/Application/Presentation file contains a provisional data ID literal.
- A later CSV row overrides an earlier row with the same address.
- The sample scenario reproduces waiting, machining, abnormal interruption, communication loss, and recovery.
- Invalid CSV fails closed and cannot enable write operations.
- Debug/Release × Win32/x64 build and test successfully.
