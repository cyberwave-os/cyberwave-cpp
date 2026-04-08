#include "cyberwave/exceptions.h"
#include "cyberwave/manifest.h"

#include <cassert>

using namespace cyberwave;

void test_manifest_defaults_and_effective_install()
{
    const auto manifest = ManifestSchema::from_json(nlohmann::json::object());
    assert(manifest.version == "1");
    assert(manifest.profile_slug == "default");
    assert(manifest.gpu == false);
    assert(!manifest.effective_install().has_value());

    const auto with_install = ManifestSchema::from_json({{"install", "pip install A"}});
    assert(with_install.effective_install() == std::optional<std::string>("pip install A"));

    const auto with_script = ManifestSchema::from_json({{"install_script", "./install.sh"}});
    assert(with_script.effective_install() == std::optional<std::string>("./install.sh"));
}

void test_manifest_validation_rules()
{
    const auto manifest =
        ManifestSchema::from_json({{"runtime", "ultralytics"}, {"model", "yolov8n.pt"}, {"input", "image"}});
    assert(manifest.runtime == std::optional<std::string>("ultralytics"));
    assert(manifest.input == std::optional<std::vector<std::string>>({std::string("image")}));

    bool bad_version = false;
    try
    {
        (void)ManifestSchema::from_json({{"version", "99"}});
    }
    catch (const CyberwaveError&)
    {
        bad_version = true;
    }
    assert(bad_version);

    bool runtime_without_model = false;
    try
    {
        (void)ManifestSchema::from_json({{"runtime", "ultralytics"}});
    }
    catch (const CyberwaveError&)
    {
        runtime_without_model = true;
    }
    assert(runtime_without_model);
}

void test_detect_dispatch_mode()
{
    assert(detect_dispatch_mode("inference.py") == "module");
    assert(detect_dispatch_mode("./models/inference.py") == "module");
    assert(detect_dispatch_mode("python server.py --params {body}") == "shell");
    assert(detect_dispatch_mode("inference.py --extra arg") == "shell");
}

int main()
{
    test_manifest_defaults_and_effective_install();
    test_manifest_validation_rules();
    test_detect_dispatch_mode();
    return 0;
}
