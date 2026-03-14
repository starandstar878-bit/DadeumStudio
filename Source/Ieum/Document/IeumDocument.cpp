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
        bool indexNeedsRebuild = (b->source != spec.source || b->target != spec.target);
        *b = spec;
        if (indexNeedsRebuild)
            rebuildIndex();
    }
}

IeumBindingGroup* IeumDocument::findGroup(const GroupId& id) noexcept
{
    for (auto& g : groups)
    {
        if (g.id == id)
            return &g;
    }
    return nullptr;
}

void IeumDocument::addGroup(const IeumBindingGroup& group)
{
    groups.push_back(group);
    touch();
}

void IeumDocument::removeGroup(const GroupId& id)
{
    auto it = std::find_if(groups.begin(), groups.end(),
                           [&](const IeumBindingGroup& g) { return g.id == id; });
    if (it != groups.end())
    {
        groups.erase(it);
        touch();
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
    groups.clear();
    idToIndex.clear();
    sourceToIndex.clear();
    targetToIndex.clear();
    touch();
}

std::vector<BindingId> IeumDocument::findBindingsBySource(const IeumEndpoint& source) const
{
    std::vector<BindingId> result;
    auto key = getEndpointKey(source);
    auto range = sourceToIndex.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
        result.push_back(bindings[it->second].id);
    return result;
}

std::vector<BindingId> IeumDocument::findBindingsByTarget(const IeumEndpoint& target) const
{
    std::vector<BindingId> result;
    auto key = getEndpointKey(target);
    auto range = targetToIndex.equal_range(key);
    for (auto it = range.first; it != range.second; ++it)
        result.push_back(bindings[it->second].id);
    return result;
}

void IeumDocument::rebuildIndex() noexcept
{
    idToIndex.clear();
    sourceToIndex.clear();
    targetToIndex.clear();

    const size_t count = bindings.size();
    idToIndex.reserve(count);
    
    for (size_t i = 0; i < count; ++i)
    {
        const auto& b = bindings[i];
        idToIndex[b.id] = i;
        sourceToIndex.emplace(getEndpointKey(b.source), i);
        targetToIndex.emplace(getEndpointKey(b.target), i);
    }
}

juce::String IeumDocument::getEndpointKey(const IeumEndpoint& ep)
{
    return ep.providerId.toString() + ":" + ep.entityId + ":" + ep.paramId;
}

void IeumDocument::touch() noexcept
{
    ++documentRevision;
}

void IeumDocument::notifyBindingStatusChanged(const BindingId &id) {
  listeners.call(&Listener::onIeumBindingStatusChanged, *this, id);
}

} // namespace Ieum
