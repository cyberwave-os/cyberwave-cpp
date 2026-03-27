/*
    capture_frame.cpp — Frame capture example

    Mirrors: examples/capture_frame.py

    Demonstrates:
     - Fetching the latest RGB frame bytes from a twin's camera sensor
     - Writing raw JPEG bytes to a file

    Python note:
        The Python example uses OpenCV / PIL / NumPy for decoding and
        manipulation (cv2.imdecode, np.hstack, PIL.Image.open, etc.).
        Those are Python-specific libraries with no standard C++ equivalent.

        In C++, get_latest_frame() returns the raw JPEG bytes as a
        std::vector<unsigned char>. To decode and display/process them
        link against an image library of your choice:
            - OpenCV:        cv::imdecode(cv::Mat(bytes), cv::IMREAD_COLOR)
            - stb_image:     stbi_load_from_memory(...)
            - libjpeg-turbo: tjDecompressHeader3(...)

    Note on the generated REST client:
        The backend endpoint returns the raw image body, but the generated
        cpprestsdk typed client declares the response as void, so
        get_latest_frame() currently returns an empty vector. The call
        still validates connectivity and permissions.

    Before running:
        export CYBERWAVE_API_KEY=your_token_here
*/

#include <cyberwave/client.h>
#include <cyberwave/config.h>
#include <cyberwave/exceptions.h>
#include <cyberwave/joints.h>
#include <cyberwave/twin.h>
#include <cyberwave/twins.h>

#include <fstream>
#include <iostream>
#include <vector>

int main()
{
    try
    {
        cyberwave::Config cfg;
        cfg.load_from_environment();
        cyberwave::Client cw(cfg);

        // Discover available twins and use the first one
        auto twins = cw.twins().list();
        if (twins.empty())
        {
            std::cout << "No twins found.\n";
            return 0;
        }
        const std::string& twin_uuid = twins[0].uuid();
        std::cout << "Using twin: " << twins[0].name() << " (" << twin_uuid << ")\n";

        cyberwave::Twin robot = cw.twin(twin_uuid);

        // --- Single frame ---
        auto frame_bytes = robot.get_latest_frame(/*mock=*/false);
        std::cout << "Frame size: " << frame_bytes.size() << " bytes\n";

        if (!frame_bytes.empty())
        {
            std::ofstream out("frame.jpg", std::ios::binary);
            out.write(reinterpret_cast<const char*>(frame_bytes.data()),
                      static_cast<std::streamsize>(frame_bytes.size()));
            std::cout << "Saved frame.jpg\n";
        }
        else
        {
            std::cout << "(No frame data returned — REST client limitation, see comment above)\n";
        }

        // --- Specific camera sensor on a multi-camera twin ---
        auto wrist_frame = robot.get_latest_frame(/*mock=*/false, /*sensor_id=*/"wrist_camera");
        std::cout << "Wrist camera frame size: " << wrist_frame.size() << " bytes\n";

        cw.disconnect();
    }
    catch (const cyberwave::CyberwaveError& e)
    {
        std::cerr << "SDK error: " << e.what() << "\n";
        return 1;
    }
    catch (const std::exception& e)
    {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
