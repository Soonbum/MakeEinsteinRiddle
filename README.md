# MakeEinsteinRiddle

아인슈타인 가옥 퀴즈 (Einstein's Riddle) 생성 프로그램입니다. C++/Dear ImGui 프레임워크를 활용했습니다.

# 환경 설정 방법

## 단계별로 설명해 드립니다.

### 1단계: vcpkg 설치 (Git 사용)

  * vcpkg는 별도의 설치 파일(.exe)이 아니라 소스 코드를 내려받아 빌드하는 방식입니다.

  * 명령 프롬프트(CMD) 또는 PowerShell을 관리자 권한으로 실행합니다.

  * vcpkg를 설치할 적당한 경로로 이동합니다. (예: C:\dev)

  * 아래 명령어를 순서대로 입력하세요.

    ```
    # 1. vcpkg 소스 코드 내려받기
    git clone https://github.com/microsoft/vcpkg.git

    # 2. vcpkg 폴더로 이동
    cd vcpkg

    # 3. vcpkg 빌드 (vcpkg.exe 파일 생성)
    .\bootstrap-vcpkg.bat
    ```

### 2단계: Visual Studio와 영구 연결 (Integration)

  * 이 명령어를 실행하면, 앞으로 만드는 모든 Visual Studio 프로젝트에서 vcpkg로 설치한 라이브러리를 자동으로 인식합니다. (Include, Lib 설정을 수동으로 안 해도 됩니다)

    ```
    .\vcpkg integrate install
    ```

    - 성공 메시지: Applied user-wide integration for this vcpkg root.라는 문구가 나오면 성공입니다.

    - 이제 어느 프로젝트에서든 #include <glfw3.h>를 쓰면 VS가 알아서 찾아줍니다.

### 3단계: 필요한 라이브러리 설치

  * 꿀팁: ImGui를 설치할 때 뒤에 [glfw-binding,...]를 붙여주면, 백엔드 파일(imgui_impl_glfw.cpp 등)까지 vcpkg가 알아서 관리해 줍니다.

    ```
    # x64 환경용으로 설치 (최근엔 64비트가 기본입니다)
    .\vcpkg install glfw3:x64-windows
    .\vcpkg install glew:x64-windows
    .\vcpkg install imgui[glfw-binding,opengl3-binding]:x64-windows
    ```

### 4단계: Visual Studio에서 사용하기

  * vcpkg 연동이 끝났다면, 이제 프로젝트 설정에서 Include 경로, Lib 경로를 직접 적을 필요가 없습니다.

  * Visual Studio에서 빈 프로젝트를 만듭니다.

  * 프로젝트 속성(Property) -> vcpkg 항목이 생긴 것을 확인합니다.

    - Use Vcpkg가 Yes로 되어 있는지 확인합니다.

  * main.cpp를 만들고 바로 코드를 작성하세요.
