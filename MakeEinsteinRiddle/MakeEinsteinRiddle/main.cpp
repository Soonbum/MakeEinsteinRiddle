#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <cstdlib>
#include <ctime>
#include <vector>
#include <string>
#include <set>
#include <GLFW/glfw3.h> 

#pragma execution_character_set("utf-8") // 이 파일의 문자열을 UTF-8로 처리하라는 지시

// 퀴즈 데이터
struct QuizConfig {
    int rowCount = 5;
    int colCount = 5;
    const std::vector<std::string> Headers = { u8"이름", u8"집 위치 [숫자, 1부터]", u8"동물", u8"음식", u8"자동차" };
    bool isHeaderSelected[5] = { false, false, false, false, false };
    char gridData[5][5][256] = {};
    bool hasError = false;
    std::string errorMessage = "";
};
static QuizConfig g_Quiz;

// QuizConfig 변수 검증하는 함수
static void validateQuizData(QuizConfig& config, const std::vector<int>& activeIndices) {
    config.hasError = false;
    config.errorMessage = "";
    int n = (int)activeIndices.size();

    for (int colIdx = 0; colIdx < n; colIdx++) {
        int actualHeaderIdx = activeIndices[colIdx];
        std::set<std::string> duplicateCheck;
        std::set<int> housePositions; // 집 위치 체크용

        for (int row = 0; row < n; row++) {
            std::string value = config.gridData[row][actualHeaderIdx];

            // 빈 값 체크
            if (value.empty()) {
                config.hasError = true;
                config.errorMessage = u8"모든 칸을 채워야 합니다. (빈 칸 발견)";
                return;
            }

            // 중복 체크
            if (duplicateCheck.find(value) != duplicateCheck.end()) {
                config.hasError = true;
                config.errorMessage = u8"중복된 값이 존재합니다: " + value;
                return;
            }
            duplicateCheck.insert(value);

            // 집 위치 특수 체크 (헤더 인덱스가 1인 경우 - "집 위치")
            if (actualHeaderIdx == 1) {
                try {
                    int pos = std::stoi(value);
                    housePositions.insert(pos);
                }
                catch (...) {
                    config.hasError = true;
                    config.errorMessage = u8"집 위치는 숫자여야 합니다: " + value;
                    return;
                }
            }
        }

        // 집 위치 연속성 및 1부터 시작 여부 체크
        if (actualHeaderIdx == 1) {
            for (int i = 1; i <= n; i++) {
                if (housePositions.find(i) == housePositions.end()) {
                    config.hasError = true;
                    config.errorMessage = u8"집 위치는 1부터 시작하는 연속된 숫자여야 합니다. (예: 1, 2, 3)";
                    return;
                }
            }
        }
    }
}

// 단서 정보를 담을 구조체
struct QuizClue {
    std::string text;
};

// 퀴즈 상태 관리를 위한 전역 변수
static std::vector<QuizClue> g_GeneratedClues;
static bool g_QuizGenerated = false;

// 문제 후보를 저장할 전역 변수
static std::vector<std::string> g_GeneratedQuestions;

// ---------------------------------------------------------
// 자연스러운 문장 생성을 위한 헬퍼 함수들
// ---------------------------------------------------------
// 1. 인물을 지칭하는 명사구 생성 (예: "개를 키우는 사람", "John")
static std::string GetPersonDescription(const std::string& header, const std::string& value) {
    if (header.find(u8"이름") != std::string::npos) return value; // '이름'은 "사람"을 붙이지 않고 그대로 반환
    if (header.find(u8"동물") != std::string::npos) return value + u8"을(를) 키우는 사람";
    if (header.find(u8"음식") != std::string::npos) return value + u8"을(를) 즐겨 먹는 사람";
    if (header.find(u8"자동차") != std::string::npos) return value + u8"을(를) 타는 사람";
    if (header.find(u8"위치") != std::string::npos) return value + u8"번째 집에 사는 사람";

    // 예기치 않은 추가 속성일 경우 (예: 직업 - 의사 -> "직업이(가) 의사인 사람")
    return header + u8"이(가) " + value + u8"인 사람";
}

// 2. 문장을 끝맺는 서술어 생성 (예: "개를 키웁니다.", "John입니다.")
static std::string GetEndPredicate(const std::string& header, const std::string& value) {
    if (header.find(u8"이름") != std::string::npos) return value + u8"입니다.";
    if (header.find(u8"동물") != std::string::npos) return value + u8"을(를) 키웁니다.";
    if (header.find(u8"음식") != std::string::npos) return value + u8"을(를) 좋아합니다.";
    if (header.find(u8"자동차") != std::string::npos) return value + u8"을(를) 타고 다닙니다.";
    if (header.find(u8"위치") != std::string::npos) return value + u8"번째 집에 삽니다.";

    // 예기치 않은 추가 속성일 경우 (예: 직업 - 의사 -> "직업이(가) 의사입니다.")
    return header + u8"이(가) " + value + u8"입니다.";
}

// ---------------------------------------------------------
// 단서 생성 함수
// ---------------------------------------------------------
static void generateLogicClues(QuizConfig& config, const std::vector<int>& activeIndices) {
    g_GeneratedClues.clear();
    int n = (int)activeIndices.size();
    if (n < 2) return;

    srand((unsigned int)time(NULL));

    // 데이터 사용 여부를 추적하는 2차원 배열 (최대 5x5)
    bool used[5][5] = { false };

    // 1. [기본 단서] 이름과 특정 속성을 연결
    for (int r = 0; r < n; r++) {
        int localColIdx = 1 + (rand() % (n - 1));
        int actualCol = activeIndices[localColIdx];

        std::string name = config.gridData[r][activeIndices[0]];
        std::string value = config.gridData[r][actualCol];
        std::string header = config.Headers[actualCol];

        g_GeneratedClues.push_back({ name + u8"은(는) " + GetEndPredicate(header, value) });

        // 사용된 데이터 마킹
        used[r][0] = true;
        used[r][localColIdx] = true;
    }

    // 2. [교차 단서] 이름이 아닌 속성끼리의 연결
    for (int r = 0; r < n; r++) {
        int localCol1 = 1 + (rand() % (n - 1));
        int localCol2 = 1 + (rand() % (n - 1));

        while (localCol1 == localCol2 && n > 2) {
            localCol2 = 1 + (rand() % (n - 1));
        }

        int actualCol1 = activeIndices[localCol1];
        int actualCol2 = activeIndices[localCol2];

        std::string val1 = config.gridData[r][actualCol1];
        std::string header1 = config.Headers[actualCol1];
        std::string val2 = config.gridData[r][actualCol2];
        std::string header2 = config.Headers[actualCol2];

        std::string clueText = GetPersonDescription(header1, val1) + u8"은(는) " + GetEndPredicate(header2, val2);
        g_GeneratedClues.push_back({ clueText });

        // 사용된 데이터 마킹
        used[r][localCol1] = true;
        used[r][localCol2] = true;
    }

    // 3. [위치 단서] 옆집 관계 생성
    int houseColLocalIdx = -1;
    for (int i = 0; i < n; i++) if (activeIndices[i] == 1) houseColLocalIdx = i;

    if (houseColLocalIdx != -1) {
        for (int r = 0; r < n; r++) {
            int currentPos = std::stoi(config.gridData[r][1]);

            if (currentPos < n) {
                int nextRow = -1;
                for (int j = 0; j < n; j++) {
                    if (std::stoi(config.gridData[j][1]) == currentPos + 1) nextRow = j;
                }

                if (nextRow != -1) {
                    int localCol1 = rand() % n;
                    int localCol2 = rand() % n;

                    if (localCol1 == houseColLocalIdx) localCol1 = 0;
                    if (localCol2 == houseColLocalIdx) localCol2 = 0;

                    int actualCol1 = activeIndices[localCol1];
                    int actualCol2 = activeIndices[localCol2];

                    std::string val1 = config.gridData[r][actualCol1];
                    std::string header1 = config.Headers[actualCol1];
                    std::string val2 = config.gridData[nextRow][actualCol2];
                    std::string header2 = config.Headers[actualCol2];

                    std::string clueText = GetPersonDescription(header1, val1) + u8"의 바로 옆집에는 " +
                        GetPersonDescription(header2, val2) + u8"이(가) 삽니다.";
                    g_GeneratedClues.push_back({ clueText });

                    // 사용된 데이터 마킹
                    used[r][localCol1] = true;
                    used[nextRow][localCol2] = true;
                }
            }
        }
    }

    // 4. [누락 방지 보완 로직] 한 번도 사용되지 않은 데이터를 찾아 강제 연결
    for (int r = 0; r < n; r++) {
        for (int c = 1; c < n; c++) { // 0번(이름)은 항상 쓰이므로 1번 열부터 검사
            if (!used[r][c]) {
                int actualCol = activeIndices[c];
                std::string name = config.gridData[r][activeIndices[0]];
                std::string value = config.gridData[r][actualCol];
                std::string header = config.Headers[actualCol];

                // 누락된 정보를 이름과 강제 연결하여 단서 추가
                g_GeneratedClues.push_back({ name + u8"은(는) " + GetEndPredicate(header, value) });
                used[r][c] = true; // 처리 완료 마킹
            }
        }
    }

    // 단서 순서를 무작위로 섞기
    for (int i = 0; i < (int)g_GeneratedClues.size(); i++) {
        int swapIdx = rand() % g_GeneratedClues.size();
        std::swap(g_GeneratedClues[i], g_GeneratedClues[swapIdx]);
    }

    // ---------------------------------------------------------
    // 5. [최종 문제 생성] 단서에 직접 나오지 않은 관계를 질문으로 만들기
    // ---------------------------------------------------------
    g_GeneratedQuestions.clear();
    int maxQuestions = 5; // 생성할 질문 개수 (원하시면 늘려도 됩니다)
    int attempts = 0;

    // 무한 루프 방지를 위해 최대 100번만 시도
    while (g_GeneratedQuestions.size() < maxQuestions && attempts < 100) {
        attempts++;

        // 표 안에서 무작위 행과 서로 다른 두 열을 선택
        int r = rand() % n;
        int localCol1 = rand() % n;
        int localCol2 = rand() % n;

        if (localCol1 == localCol2) continue;

        int actualCol1 = activeIndices[localCol1];
        int actualCol2 = activeIndices[localCol2];

        std::string val1 = config.gridData[r][actualCol1];
        std::string header1 = config.Headers[actualCol1];
        std::string val2 = config.gridData[r][actualCol2];
        std::string header2 = config.Headers[actualCol2];

        // [핵심] 생성된 단서들 중에 val1과 val2가 "동시에" 들어간 문장이 있는지 확인
        bool isDirectlyMentioned = false;
        for (const auto& clue : g_GeneratedClues) {
            if (clue.text.find(val1) != std::string::npos && clue.text.find(val2) != std::string::npos) {
                isDirectlyMentioned = true;
                break;
            }
        }

        // 직접적인 언급이 단 하나도 없다면, 논리적 추론이 필요한 좋은 문제입니다!
        if (!isDirectlyMentioned) {
            std::string qText;

            if (actualCol2 == 0) {
                // 정답이 '이름'인 경우 더 자연스럽게 처리
                qText = u8"질문: " + GetPersonDescription(header1, val1) + u8"은(는) 누구일까요? (정답: " + val2 + u8")";
            }
            else {
                // 그 외의 경우
                qText = u8"질문: " + GetPersonDescription(header1, val1) + u8"의 " + header2 + u8"은(는) 무엇일까요? (정답: " + val2 + u8")";
            }

            // 동일한 질문이 이미 생성되었는지 확인 후 추가
            bool isDuplicate = false;
            for (const auto& existingQ : g_GeneratedQuestions) {
                if (existingQ == qText) isDuplicate = true;
            }

            if (!isDuplicate) {
                g_GeneratedQuestions.push_back(qText);
            }
        }
    }

    g_QuizGenerated = true;
}

static void glfw_error_callback(int error, const char* description)
{
    fprintf(stderr, "Glfw Error %d: %s\n", error, description);
}

int main(int, char**)
{
    // GLFW 초기화 및 에러 콜백 설정
    glfwSetErrorCallback(glfw_error_callback);
    if (!glfwInit()) return 1;

    // OpenGL 3.3 Core Profile 설정
    const char* glsl_version = "#version 130";
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);  // 3.2+ 전용
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);            // Mac 지원용

    // 윈도우 생성
    GLFWwindow* window = glfwCreateWindow(1280, 720, "Einstein Logic Quiz - OpenGL3", NULL, NULL);
    if (window == NULL) return 1;
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1); // V-Sync 활성화

    // ImGui 컨텍스트 생성 및 설정
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 키보드 탐색 활성화

    // 테마 설정
    ImGui::StyleColorsDark();

    // 한글 폰트 설정
    // 윈도우의 '맑은 고딕' 경로를 사용합니다. (영문 계정이라도 폰트 경로는 동일합니다)
    const char* font_path = "C:\\Windows\\Fonts\\malgun.ttf";

    // 폰트 설정 부분 수정
    ImFontConfig config;
    config.MergeMode = false; // 기본값

    ImFont* font = io.Fonts->AddFontFromFileTTF(font_path, 16.0f, &config, io.Fonts->GetGlyphRangesKorean());

    if (font == nullptr) {
        // 만약 여기서 실패한다면 폰트 파일에 접근 권한이 없거나 경로가 틀린 것입니다.
        // 폰트 파일을 프로젝트 폴더로 복사한 뒤 "./malgun.ttf"로 시도해 보세요.
        printf("Font Load Failed!\n");
    }
    else {
        // 폰트가 로드되었다면 이 폰트를 기본 폰트로 설정
        io.FontDefault = font;
    }

    // 백엔드 바인딩 초기화 (GLFW + OpenGL3)
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // 배경색 설정
    ImVec4 clear_color = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);

    // 메인 루프
    while (!glfwWindowShouldClose(window))
    {
        // 이벤트 폴링 (키보드, 마우스 입력 등)
        glfwPollEvents();

        // ImGui 프레임 시작
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        // ---------------------------------------------------------
        // UI 구축 구역
        // ---------------------------------------------------------
        {
            // 위젯 생성
            if (font != nullptr) ImGui::PushFont(font);
            ImGui::SetNextWindowSize(ImVec2(800, 500), ImGuiCond_FirstUseEver);
            ImGui::Begin(u8"퀴즈 생성기");

            // ---------------------------------------------------------
            // 설정 영역
            // ---------------------------------------------------------
            ImGui::Checkbox(g_Quiz.Headers[0].c_str(), &g_Quiz.isHeaderSelected[0]); ImGui::SameLine();
            ImGui::Checkbox(g_Quiz.Headers[1].c_str(), &g_Quiz.isHeaderSelected[1]); ImGui::SameLine();
            ImGui::Checkbox(g_Quiz.Headers[2].c_str(), &g_Quiz.isHeaderSelected[2]); ImGui::SameLine();
            ImGui::Checkbox(g_Quiz.Headers[3].c_str(), &g_Quiz.isHeaderSelected[3]); ImGui::SameLine();
            ImGui::Checkbox(g_Quiz.Headers[4].c_str(), &g_Quiz.isHeaderSelected[4]);

            // 체크된 항목 개수 계산 및 활성화된 인덱스 추출
            std::vector<int> activeIndices;
            for (int i = 0; i < 5; i++) {
                if (g_Quiz.isHeaderSelected[i]) {
                    activeIndices.push_back(i);
                }
            }

            int n = (int)activeIndices.size(); // n x n 구조의 n

            // 3개 이상 선택해야 테이블 생성됨
            if (n >= 3)
            {
                ImGui::Text(u8"입력 테이블 (%d x %d)", n, n);

                // 행 번호 출력을 위해 n+1 열로 생성
                if (ImGui::BeginTable("QuizTable", n + 1, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg))
                {
                    // 헤더 설정
                    ImGui::TableSetupColumn("No.", ImGuiTableColumnFlags_WidthFixed, 30.0f);
                    for (int i : activeIndices) {
                        ImGui::TableSetupColumn(g_Quiz.Headers[i].c_str());
                    }
                    ImGui::TableHeadersRow();

                    // n개의 행 생성 (n x n 구조)
                    for (int row = 0; row < n; row++)
                    {
                        ImGui::TableNextRow();

                        // 0번 열: 행 번호
                        ImGui::TableSetColumnIndex(0);
                        ImGui::Text("%d", row + 1);

                        // 데이터 열들
                        for (int col = 0; col < n; col++)
                        {
                            ImGui::TableSetColumnIndex(col + 1);

                            // 실제 데이터 인덱스 매핑
                            int actualHeaderIdx = activeIndices[col];

                            // ImGui ID 충돌 방지를 위한 고유 레이블 생성
                            char buf[64];
                            sprintf_s(buf, "##cell_%d_%d", row, actualHeaderIdx);

                            // 너비를 꽉 채우기
                            ImGui::PushItemWidth(-FLT_MIN);
                            ImGui::InputText(buf, g_Quiz.gridData[row][actualHeaderIdx], 256);
                            ImGui::PopItemWidth();
                        }
                    }
                    ImGui::EndTable();
                }
            }
            else {
                ImGui::TextDisabled(u8"상단에서 3개 이상 조건을 선택하면 테이블이 나타납니다.");
            }
            
            // ---------------------------------------------------------
            // 퀴즈 생성 및 검증 버튼 영역
            // ---------------------------------------------------------
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            if (ImGui::Button(u8"퀴즈 생성 및 검증", ImVec2(150, 40)))
            {
                validateQuizData(g_Quiz, activeIndices);
                if (!g_Quiz.hasError) {
                    generateLogicClues(g_Quiz, activeIndices); // 검증 성공 시 단서 생성
                }
                else {
                    g_QuizGenerated = false;
                }
            }

            // 검증 결과 알림창
            if (!g_Quiz.errorMessage.empty())
            {
                ImGui::Spacing();
                if (g_Quiz.hasError)
                {
                    // 에러가 있는 경우: 빨간색 메시지
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
                    ImGui::Text(u8"검증 실패: %s", g_Quiz.errorMessage.c_str());
                    ImGui::PopStyleColor();
                }
                else
                {
                    // 성공한 경우: 녹색 메시지
                    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
                    ImGui::Text(u8"검증 성공! 모든 데이터가 올바릅니다.");
                    ImGui::PopStyleColor();
                }
            }

            // ---------------------------------------------------------
            // 생성된 문장 (Clues) 출력 영역
            // ---------------------------------------------------------            
            if (g_QuizGenerated && !g_Quiz.hasError)
            {
                ImGui::Spacing();
                ImGui::TextColored(ImVec4(1.0f, 1.0f, 0.0f, 1.0f), u8"--> 생성된 단서 (이 정보로 문제를 푸세요):");
                ImGui::Indent();

                for (int i = 0; i < (int)g_GeneratedClues.size(); i++) {
                    ImGui::BulletText("%s", g_GeneratedClues[i].text.c_str());
                }

                ImGui::Unindent();
                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();

                // 새로 추가된 퀴즈 문제 출력 영역
                ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.8f, 1.0f), u8"퀴즈 문제 (논리적 추론 필요):");
                ImGui::Indent();
                for (int i = 0; i < (int)g_GeneratedQuestions.size(); i++) {
                    ImGui::BulletText("%s", g_GeneratedQuestions[i].c_str());
                }
                ImGui::Unindent();

                ImGui::Spacing();
                if (ImGui::Button(u8"단서 및 문제 텍스트 복사")) {
                    std::string allText = u8"[논리 퀴즈 단서]\n";
                    for (auto& c : g_GeneratedClues) allText += "- " + c.text + "\n";
                    allText += u8"\n[문제]\n";
                    for (auto& q : g_GeneratedQuestions) allText += "- " + q + "\n";
                    ImGui::SetClipboardText(allText.c_str());
                }
            }

            // ---------------------------------------------------------
            // 디버깅 모니터
            // ---------------------------------------------------------
            ImGui::Spacing();
            ImGui::Separator();
            if (ImGui::TreeNode(u8"디버그: g_Quiz 내부 데이터 모니터"))
            {
                ImGui::Text(u8"선택된 헤더 개수(n): %d", (int)activeIndices.size());

                // 현재 체크박스 상태 확인
                std::string activeStr = "";
                for (int i = 0; i < 5; i++) {
                    if (g_Quiz.isHeaderSelected[i]) {
                        activeStr += g_Quiz.Headers[i] + " ";
                    }
                }
                ImGui::Text(u8"활성 헤더: [ %s ]", activeStr.c_str());

                ImGui::Spacing();

                // 실제 gridData 배열 내용 출력 (값이 있는 것만)
                ImGui::Text(u8"메모리 상의 입력 데이터:");
                ImGui::Indent(); // 들여쓰기 시작
                for (int r = 0; r < 5; r++) {
                    std::string rowStr = "Row " + std::to_string(r) + ": ";
                    bool rowHasData = false;
                    for (int c = 0; c < 5; c++) {
                        if (g_Quiz.gridData[r][c][0] != '\0') { // 데이터가 비어있지 않으면
                            rowStr += "[" + std::string(g_Quiz.gridData[r][c]) + "] ";
                            rowHasData = true;
                        }
                    }
                    if (rowHasData) {
                        ImGui::Text("%s", rowStr.c_str());
                    }
                }
                ImGui::Unindent(); // 들여쓰기 끝

                if (ImGui::Button(u8"데이터 전체 초기화")) {
                    memset(g_Quiz.gridData, 0, sizeof(g_Quiz.gridData));
                }

                ImGui::TreePop();
            }

            // 위젯 종료
            ImGui::End();
            if (font != nullptr) ImGui::PopFont();
        }
        // ---------------------------------------------------------

        // 렌더링 과정
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
    }

    // 리소스 정리
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}
