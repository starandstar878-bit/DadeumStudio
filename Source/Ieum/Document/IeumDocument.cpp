#include "IeumDocument.h"
#include "IeumDocumentHistory.h"

namespace Ieum {

IeumDocument::IeumDocument()
    : history(std::make_unique<IeumDocumentHistory>()) {}

IeumDocument::~IeumDocument() = default;

IeumBindingSpec *IeumDocument::findBinding(const BindingId &id) noexcept {
  for (auto &b : bindings) {
    if (b.id == id)
      return &b;
  }
  return nullptr;
}

const IeumBindingSpec *
IeumDocument::findBinding(const BindingId &id) const noexcept {
  for (const auto &b : bindings) {
    if (b.id == id)
      return &b;
  }
  return nullptr;
}

BindingId IeumDocument::allocBindingId() noexcept {
  return "binding-" + juce::String(nextIdCounter++);
}

void IeumDocument::executeCommand(std::unique_ptr<IeumCommand> command) {
  if (history && command != nullptr)
    {
        history->pushNext(std::move(command), *this);
        touch();
        listeners.call(&Listener::onIeumDocumentChanged, *this);
    }
}

bool IeumDocument::undo() {
  if (history && history->undo(*this))
    {
        touch();
        listeners.call(&Listener::onIeumDocumentChanged, *this);
        return true;
    }
    return false;
}

bool IeumDocument::redo() {
  if (history && history->redo(*this))
    {
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

void IeumDocument::touch() noexcept
{
    ++documentRevision;
}

void IeumDocument::notifyBindingStatusChanged(const BindingId& id)
{
    listeners.call(&Listener::onIeumBindingStatusChanged, *this, id);
}

} // namespace Ieum
