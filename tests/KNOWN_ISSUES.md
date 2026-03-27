# Known Issues - C++ SDK

## Compilation Error in Generated SDK

### Issue

The auto-generated REST SDK fails to compile with the following error:

```
/sdk/rest/src/api/DefaultApi.cpp:4529:105: error: no matching function for call to
'org::openapitools::client::api::ApiClient::parameterToString(std::map<std::__cxx11::basic_string<char>,
std::shared_ptr<org::openapitools::client::model::AnyType> >&)'
```

### Root Cause

The OpenAPI generator (`openapitools/openapi-generator-cli`) generates code for the endpoint:

- `POST /api/v1/environments/{uuid}/simulations`
- Function: `srcAppApiEnvironmentsCreateEnvironmentSimulation`

This endpoint has a query parameter `payload` of type `object` (map/dictionary), but the generator doesn't create a `parameterToString` overload for `std::map` types.

### Affected Code

File: `cyberwave-cpp-sdk/rest/src/api/DefaultApi.cpp`
Line: ~4529

```cpp
localVarQueryParams[utility::conversions::to_string_t("payload")] =
    ApiClient::parameterToString(*payload);
```

### Workaround Options

#### Option 1: Fix the Backend API (Recommended)

The `payload` parameter should be in the request body, not as a query parameter. Query parameters should be simple types (strings, numbers, booleans), not complex objects.

**Backend Change:**

```python
# Change from query parameter to request body
@router.post("/environments/{uuid}/simulations")
def create_environment_simulation(
    uuid: str,
    payload: Optional[Dict[str, Any]] = Body(None)  # Not Query
):
    ...
```

#### Option 2: Patch the Generated Code

After generating the SDK, manually patch the problematic line:

```cpp
// Before (line ~4529 in DefaultApi.cpp)
localVarQueryParams[utility::conversions::to_string_t("payload")] =
    ApiClient::parameterToString(*payload);

// After - serialize the map to JSON string
if (payload) {
    web::json::value jsonPayload = web::json::value::object();
    for (const auto& [key, value] : *payload) {
        if (value) {
            jsonPayload[utility::conversions::to_string_t(key)] = value->toJson();
        }
    }
    localVarQueryParams[utility::conversions::to_string_t("payload")] =
        jsonPayload.serialize();
}
```

#### Option 3: Add parameterToString Overload

Add a new overload to `ApiClient.cpp`:

```cpp
utility::string_t ApiClient::parameterToString(
    const std::map<utility::string_t, std::shared_ptr<ModelBase>>& value)
{
    web::json::value jsonValue = web::json::value::object();
    for (const auto& [key, val] : value) {
        if (val) {
            jsonValue[key] = val->toJson();
        }
    }
    return jsonValue.serialize();
}
```

And declare it in `ApiClient.h`:

```cpp
static utility::string_t parameterToString(
    const std::map<utility::string_t, std::shared_ptr<ModelBase>>& value);
```

### Temporary Solution

For testing purposes, you can comment out the problematic endpoint in the generated code or regenerate the SDK after fixing the backend API.

### Status

- **Reported:** 2025-11-11
- **Fixed:** 2025-11-11
- **Severity:** High (blocks SDK compilation)
- **Resolution:** Created `SimulationPayloadSchema` and updated endpoint to use it as a request body parameter

### Fix Applied

The backend has been updated with:

1. **New Schema** (`cyberwave-backend/src/app/api/schemas.py`):

   ```python
   class SimulationPayloadSchema(Schema):
       """Schema for simulation creation payload."""
       duration: float = 10.0
       stream_data: bool = True
   ```

2. **Updated Endpoint** (`cyberwave-backend/src/app/api/environments.py`):
   ```python
   @router.post("/{uuid}/simulations", response=dict[str, Any])
   def create_environment_simulation(
       request: Request, uuid: str, payload: SimulationPayloadSchema | None = None
   ) -> dict[str, Any]:
       # ... implementation using payload.duration and payload.stream_data
   ```

After regenerating the SDK, the compilation error should be resolved.

### Related Files

- `cyberwave-backend/src/app/api/environments.py` - Backend endpoint definition
- `cyberwave-cpp-sdk/rest/src/api/DefaultApi.cpp` - Generated SDK code
- `cyberwave-cpp-sdk/rest/include/CppRestOpenAPIClient/ApiClient.h` - API client header
