#include "IIeumDocument.h"
#include "IIeumDocumentHistory.h"

namespace Ieum {

IIeumDocument::IIeumDocument()
    : history(std::make_unique<IIeumDocumentHistory>())
{
}

IIeumDocument::~IIeumDocument() = default;

IBindingSpec* IIeumDocument::findBinding(const BindingId& id) noexcept
{
    for (auto& b : bindings)
    {
        if (b.id == id)
            return &b;
    }
    return nullptr;
}

const IBindingSpec* IIeumDocument::findBinding(const BindingId& id) const noexcept
{
    for (const auto& b : bindings)
    {
        if (b.id == id)
            return &b;
    }
    return nullptr;
}

BindingId IIeumDocument::allocBindingId() noexcept
{
    return "binding-" + juce::String(nextIdCounter++);
}

void IIeumDocument::executeCommand(std::unique_ptr<IIeumCommand> command)
{
    if (history && command != nullptr)
    {
        history->pushNext(std::move(command), *this);
        touch();
    }
}

bool IIeumDocument::undo()
{
    if (history && history->undo(*this))
    {
        touch();
        return true;
    }
    return false;
}

bool IIeumDocument::redo()
{
    if (history && history->redo(*this))
    {
        touch();
        return true;
    }
    return false;
}

void IIeumDocument::clearHistory()
{
    if (history)
        history->clear();
}

void IIeumDocument::touch() noexcept
{
    ++documentRevision;
}

} // namespace Ieum
