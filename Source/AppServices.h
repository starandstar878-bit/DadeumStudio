// BOM
#pragma once

#include <JuceHeader.h>
#include <memory>

// =============================================================================
//  DadeumStudio 앱 공통 서비스 컨테이너
//
//  DadeumStudioApplication 이 수명을 소유하며,
//  MainComponent 생성 시 참조로 전달한다.
//  각 서브 에디터(Gyeol, Teul, ...)는 필요한 서비스만 참조로 받는다.
// =============================================================================
struct AppServices {
  // -------------------------------------------------------------------------
  //  오디오 장치 관리자 (앱 전체 단 하나)
  //
  //  JUCE 규칙: AudioDeviceManager 는 반드시 하나만 존재해야 한다.
  //  Teul::TGraphRuntime 이 AudioIODeviceCallback 으로 여기에 등록된다.
  //
  //  TODO [Phase 3 - Teul] TGraphRuntime 초기화 시:
  //    audioDeviceManager.addAudioCallback(&teulRuntime);
  //    audioDeviceManager.removeAudioCallback(&teulRuntime);  // 소멸 시
  //
  //  TODO [Phase 3 - Teul] 멀티 인풋/아웃풋:
  //    AudioDeviceManager::AudioDeviceSetup setup;
  //    setup.inputChannels  = BigInteger(0b1111);  // 채널 0~3 활성화
  //    setup.outputChannels = BigInteger(0b0011);  // 채널 0~1 활성화
  //    audioDeviceManager.setAudioDeviceSetup(setup, true);
  //
  //  TODO [Phase 3 - Teul] 장치 목록 실시간 갱신:
  //    audioDeviceManager.addChangeListener(deviceSelectorUI);
  //    // 장치 꽂힘/뽑힘 시 changeListenerCallback() 자동 호출
  // -------------------------------------------------------------------------
  juce::AudioDeviceManager audioDeviceManager;

  // -------------------------------------------------------------------------
  //  앱 전역 설정 저장소 (장치 설정, 레이아웃 등)
  //
  //  TODO: AudioDeviceManager 설정 persist
  //    auto xml = audioDeviceManager.createStateXml();
  //    // appSettings 에 직렬화해서 저장
  // -------------------------------------------------------------------------
  // juce::ApplicationProperties appSettings; // TODO: 필요 시 추가

  AppServices() {
    // 기본 오디오 장치 초기화 (입력 2채널, 출력 2채널)
    // TODO [Phase 3]: UI 에서 사용자가 장치를 선택할 수 있도록 변경
    //   → GlobalToolbar 의 오디오 장치 콤보박스와 연결
    const int numInputChannels = 2;
    const int numOutputChannels = 2;

    audioDeviceManager.initialiseWithDefaultDevices(numInputChannels,
                                                    numOutputChannels);
  }

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AppServices)
};
