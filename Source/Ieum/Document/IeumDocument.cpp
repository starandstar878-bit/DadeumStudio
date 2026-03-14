#include "IeumDocument.h"
#include "IeumDocumentHistory.h"

namespace Ieum {

IeumDocument::IeumDocument()
    : history(std::make_unique<IeumDocumentHistory>()) {}

IeumDocument::~IeumDocument() = default;

IeumBindingSpec* IeumDocument::findBinding(const BindingId& id) noexcept
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return &bindings[it->second];
        
    return nullptr;
}

const IeumBindingSpec* IeumDocument::findBinding(const BindingId& id) const noexcept
{
    auto it = idToIndex.find(id);
    if (it != idToIndex.end())
        return &bindings[it->second];

    return nullptr;
}

BindingId IeumDocument::allocBindingId() noexcept {
  return "binding-" + juce::String(nextIdCounter++);
}

void IeumDocument::addBinding(const IeumBindingSpec& spec)
{
    bindings.push_back(spec);
    rebuildIndex();
}

void IeumDocument::removeBinding(const BindingId& id)
{
    auto it = std::find_if(bindings.begin(), bindings.end(),
                           [&](const IeumBindingSpec& b) { return b.id == id; });
    if (it != bindings.end())
    {
        bindings.erase(it);
        rebuildIndex();
    }
}

void IeumDocument::updateBinding(const IeumBindingSpec& spec)
{
    if (auto* b = findBinding(spec.id))
    {
        *b = spec;
        // Index does not need rebuilding for update as ID remains same
    }
}

void IeumDocument::executeCommand(std::unique_ptr<IeumCommand> command) {
  if (history && command != nullptr) {
    history->pushNext(std::move(command), *this);
    touch();
    listeners.call(&Listener::onIeumDocumentChanged, *this);
  }
}

bool IeumDocument::undo() {
  if (history && history->undo(*this)) {
    touch();
    listeners.call(&Listener::onIeumDocumentChanged, *this);
    return true;
  }
  return false;
}

bool IeumDocument::redo() {
  if (history && history->redo(*this)) {
    touch();
    listeners.call(&Listener::onIeumDocumentChanged, *this);
    return true;
  }
  return false;
}

void IeumDocument::clearHistory() {
  if (history)
    history->clear();
}

void IeumDocument::clear() noexcept
{
    bindings.clear();
    idToIndex.clear();
    touch();
}

void IeumDocument::rebuildIndex() noexcept
{
    idToIndex.clear();
    idToIndex.reserve(bindings.size());
    for (size_t i = 0; i < bindings.size(); ++i)
        idToIndex[bindings[i].id] = i;
}

void IeumDocument::touch() noexcept
{
    ++documentRevision;
}

void IeumDocument::notifyBindingStatusChanged(const BindingId &id) {
  listeners.call(&Listener::onIeumBindingStatusChanged, *this, id);
}

} // namespace Ieum
