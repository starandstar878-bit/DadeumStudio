#include "TNaruBridge.h"
#include "Teul2/Runtime/TTeulRuntime.h"

namespace Teul {

struct TNaruBridge::Impl {
  TTeulRuntime runtime;
};

TNaruBridge::TNaruBridge() : impl(std::make_unique<Impl>()) {}
TNaruBridge::~TNaruBridge() = default;

juce::Result TNaruBridge::loadProject(const juce::File &file) {
  // TODO: TBridgeJsonCodec을 사용하여 도큐먼트 로드 및 런타임 빌드
  juce::ignoreUnused(file);
  return juce::Result::ok();
}

juce::Result TNaruBridge::saveProject(const juce::File &file) {
  // TODO: 현재 상태를 JSON으로 직렬화하여 저장
  juce::ignoreUnused(file);
  return juce::Result::ok();
}

TExportResult TNaruBridge::runExport(const TExportOptions &options) {
  // TODO: TExporter(또는 대응하는 클래스)를 사용하여 익스포트 실행
  juce::ignoreUnused(options);
  return {};
}

juce::var TNaruBridge::getEngineStatus() const {
  auto *obj = new juce::DynamicObject();
  obj->setProperty("active", true);
  return juce::var(obj);
}

} // namespace Teul
