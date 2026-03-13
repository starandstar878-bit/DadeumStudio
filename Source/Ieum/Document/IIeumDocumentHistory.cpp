#include "IIeumDocumentHistory.h"
#include "IIeumDocument.h"
#include <algorithm>

namespace Ieum {

namespace {

constexpr int kMaxHistoryCommands = 100;

class AddBindingCommand final : public IIeumCommand {
public:
    explicit AddBindingCommand(const IBindingSpec& specIn) : spec(specIn) {}

    void execute(IIeumDocument& document) override {
        addedBindingId = spec.id;
        document.bindings.push_back(spec);
    }

    void undo(IIeumDocument& document) override {
        auto it = std::find_if(document.bindings.begin(), document.bindings.end(),
                               [&](const IBindingSpec& b) { return b.id == addedBindingId; });
        if (it != document.bindings.end())
            document.bindings.erase(it);
    }

private:
    IBindingSpec spec;
    BindingId addedBindingId;
};

class RemoveBindingCommand final : public IIeumCommand {
public:
    explicit RemoveBindingCommand(const BindingId& idIn) : targetId(idIn) {}

    void execute(IIeumDocument& document) override {
        if (auto* b = document.findBinding(targetId)) {
            backup = *b;
            auto it = std::find_if(document.bindings.begin(), document.bindings.end(),
                                   [&](const IBindingSpec& candidate) { return candidate.id == targetId; });
            if (it != document.bindings.end())
                document.bindings.erase(it);
        }
    }

    void undo(IIeumDocument& document) override {
        if (!backup.id.isEmpty())
            document.bindings.push_back(backup);
    }

private:
    BindingId targetId;
    IBindingSpec backup;
};

class UpdateBindingCommand final : public IIeumCommand {
public:
    UpdateBindingCommand(const IBindingSpec& oldSpecIn, const IBindingSpec& newSpecIn)
        : oldSpec(oldSpecIn), newSpec(newSpecIn) {}

    void execute(IIeumDocument& document) override {
        if (auto* b = document.findBinding(newSpec.id))
            *b = newSpec;
    }

    void undo(IIeumDocument& document) override {
        if (auto* b = document.findBinding(oldSpec.id))
            *b = oldSpec;
    }

private:
    IBindingSpec oldSpec;
    IBindingSpec newSpec;
};

} // namespace

IIeumDocumentHistory::IIeumDocumentHistory() = default;
IIeumDocumentHistory::~IIeumDocumentHistory() = default;

void IIeumDocumentHistory::pushNext(std::unique_ptr<IIeumCommand> command, IIeumDocument& document)
{
    if (command == nullptr)
        return;

    if (currentIndex < static_cast<int>(commands.size()) - 1)
        commands.erase(commands.begin() + currentIndex + 1, commands.end());

    command->execute(document);
    commands.push_back(std::move(command));
    currentIndex = static_cast<int>(commands.size()) - 1;

    if (static_cast<int>(commands.size()) > kMaxHistoryCommands)
    {
        commands.erase(commands.begin());
        --currentIndex;
    }
}

bool IIeumDocumentHistory::undo(IIeumDocument& document)
{
    if (currentIndex < 0 || currentIndex >= static_cast<int>(commands.size()))
        return false;

    commands[static_cast<size_t>(currentIndex)]->undo(document);
    --currentIndex;
    return true;
}

bool IIeumDocumentHistory::redo(IIeumDocument& document)
{
    if (currentIndex + 1 >= static_cast<int>(commands.size()))
        return false;

    ++currentIndex;
    commands[static_cast<size_t>(currentIndex)]->execute(document);
    return true;
}

void IIeumDocumentHistory::clear()
{
    commands.clear();
    currentIndex = -1;
}

std::unique_ptr<IIeumCommand> createAddBindingCommand(const IBindingSpec& spec)
{
    return std::make_unique<AddBindingCommand>(spec);
}

std::unique_ptr<IIeumCommand> createRemoveBindingCommand(const BindingId& id)
{
    return std::make_unique<RemoveBindingCommand>(id);
}

std::unique_ptr<IIeumCommand> createUpdateBindingCommand(const IBindingSpec& oldSpec, const IBindingSpec& newSpec)
{
    return std::make_unique<UpdateBindingCommand>(oldSpec, newSpec);
}

} // namespace Ieum
