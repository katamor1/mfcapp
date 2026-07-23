# ADR 0007: Use provisional sequential data IDs and a CSV response mock

- **Status:** Accepted
- **Date:** 2026-07-24
- **Decision owner:** Product owner instruction
- **Related design:** `docs/superpowers/specs/2026-07-23-shelf-manager-architecture-design.md`
- **Related plan:** `docs/superpowers/plans/2026-07-23-shelf-manager-mvp-foundation-fake-vertical-slice.md`

## Context

The vendor-assigned COM data IDs are not available yet. Development must continue without embedding guessed production identifiers throughout the GUI. The current response source must also be deterministic and editable without a running COM server.

The application already defines the replaceable high-level ports `IMachineStateReader` and `IMachineCommandGateway`. Both the CSV-backed mock and the future COM gateway must implement these ports; Presentation and Application code must not know which adapter is active.

## Decision

Until the vendor data dictionary is supplied:

1. Use provisional data IDs starting at 1 and increasing without gaps.
2. Keep the mapping in the single header `ProvisionalDataIds.h`.
3. Read mock COM responses from `config/mock/machine-responses.csv`.
4. Use the generic CSV address shape `at_ms,data_id,sub_id1,sub_id2,value` so each row corresponds to a future `Get(dataId, subId1, subId2)` response.
5. A later row for the same address overrides its previous value from that timestamp onward.
6. Parse the CSV into `FakeScenario`, then expose it through `FakeMachineGateway`, which implements the same Application ports as the future `ComMachineGateway`.
7. Do not place provisional numeric IDs in Domain, Application, Presenter, or MFC View code.

## Provisional ID catalog

| ID | Logical data | subId1 | subId2 | Access |
|---:|---|---|---|---|
| 1 | Machine connection state | 0 | 0 | Read |
| 2 | Machine mode | 0 | 0 | Read |
| 3 | Machine error active | 0 | 0 | Read |
| 4 | Machine warning active | 0 | 0 | Read |
| 5 | Machine message | 0 | 0 | Read |
| 6 | Rack level count | 0 | 0 | Read |
| 7 | Rack position count | Rack level | 0 | Read |
| 8 | Workpiece count | 0 | 0 | Read |
| 9 | Workpiece ID by index | One-based list index | 0 | Read |
| 10 | Workpiece location type | Workpiece ID | 0 | Read |
| 11 | Workpiece location primary value | Workpiece ID | 0 | Read |
| 12 | Workpiece location secondary value | Workpiece ID | 0 | Read |
| 13 | Workpiece priority | Workpiece ID | 0 | Read/Write |
| 14 | Workpiece status | Workpiece ID | 0 | Read |
| 15 | Workpiece instruction count | Workpiece ID | 0 | Read |
| 16 | Workpiece instruction name | Workpiece ID | One-based instruction index | Read |
| 17 | Workpiece instruction order | Workpiece ID | One-based instruction index | Read/Write |
| 18 | Destination count | 0 | 0 | Read |
| 19 | Destination type | One-based destination index | 0 | Read |
| 20 | Destination primary value | One-based destination index | 0 | Read |
| 21 | Destination secondary value | One-based destination index | 0 | Read |
| 22 | Destination availability | One-based destination index | 0 | Read |
| 23 | Manual transport request | Workpiece ID | Destination index | Write |

IDs are provisional. When production IDs arrive, change the catalog and adapter contract tests rather than the use cases or views.

## CSV value vocabulary

- Connection: `connected`, `degraded`, `disconnected`, `unknown`
- Machine mode: `manual`, `automatic_scheduled`, `unknown`
- Boolean: `0`, `1`, `false`, `true`
- Workpiece status: `waiting`, `machining`, `completed`, `interrupted_abnormally`, `in_transport`, `unknown`
- Workpiece location: `rack`, `setup`, `machining`, `transport`, `unknown`
- Destination type: `rack`, `setup`, `machining`
- Destination availability: `available`, `occupied`, `unavailable`, `unknown`

CSV quoting follows the common double-quote convention; a comma-containing value may be written as `"normal, ready"`, and an embedded quote is doubled.

## Consequences

### Positive

- UI and use-case development can continue without the COM server.
- Test scenarios are source-controlled and deterministic.
- The future COM change is an adapter replacement rather than a Presentation/Application rewrite.
- Temporary IDs are auditable and cannot silently spread through the codebase.

### Negative

- The provisional catalog is not a vendor contract and must not be deployed as though it were one.
- CSV timing simulates responses but does not prove COM apartment, latency, HRESULT, BSTR ownership, or network behavior.
- Production integration still requires the vendor type library, definitive data dictionary, error semantics, and simulator or real machine.

## Verification

- `CsvScenarioLoaderTests.ProvisionalDataIdsAreSequentialFromOne`
- `CsvScenarioLoaderTests.LoadsFramesAndCarriesForwardUnchangedResponses`
- `CsvScenarioLoaderTests.RejectsDuplicateAddressAtTheSameTimestamp`
- `CsvScenarioLoaderTests.RejectsMissingRequiredResponses`
- GitHub Actions matrix: Debug/Release × Win32/x64
