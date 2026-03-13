#include "IeumDocumentHistory.h"
#include "IeumDocument.h"
#include <algorithm>

namespace Ieum {

namespace {

constexpr int kMaxHistoryCommands = 100;

class AddBindingCommand final : public IeumCommand {
public:
    explicit AddBindingCommand(const IeumBindingSpec& specIn) : spec(specIn) {}

    void execute(IeumDocument& document) override {
        addedBindingId = spec.id;
        document.bindings.push_back(spec);
    }

    void undo(IeumDocument& document) override {
        auto it = std::find_if(document.bindings.begin(), document.bindings.end(),
                               [&](const IeumBindingSpec& b) { return b.id == addedBindingId; });
        if (it != document.bindings.end())
            document.bindings.erase(it);
    }

private:
    IeumBindingSpec spec;
    BindingId addedBindingId;
};

class RemoveBindingCommand final : public IeumCommand {
public:
    explicit RemoveBindingCommand(const BindingId& idIn) : targetId(idIn) {}

    void execute(IeumDocument& document) override {
        if (auto* b = document.findBinding(targetId)) {
            backup = *b;
            auto it = std::find_if(document.bindings.begin(), document.bindings.end(),
                                   [&](const IeumBindingSpec& candidate) { return candidate.id == targetId; });
            if (it != document.bindings.end())
                document.bindings.erase(it);
        }
    }

    void undo(IeumDocument& document) override {
        if (!backup.id.isEmpty())
            document.bindings.push_back(backup);
    }

private:
    BindingId targetId;
    IeumBindingSpec backup;
};

class UpdateBindingCommand final : public IeumCommand {
public:
    UpdateBindingCommand(const IeumBindingSpec& oldSpecIn, const IeumBindingSpec& newSpecIn)
        : oldSpec(oldSpecIn), newSpec(newSpecIn) {}

    void execute(IeumDocument& document) override {
        if (auto* b = document.findBinding(newSpec.id))
            *b = newSpec;
    }

    void undo(IeumDocument& document) override {
        if (auto* b = document.findBinding(oldSpec.id))
            *b = oldSpec;
    }

private:
    IeumBindingSpec oldSpec;
    IeumBindingSpec newSpec;
};

} // namespace

IeumDocumentHistory::IeumDocumentHistory() = default;
IeumDocumentHistory::~IeumDocumentHistory() = default;

void IeumDocumentHistory::pushNext(std::unique_ptr<IeumCommand> command, IeumDocument& document)
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

bool IeumDocumentHistory::undo(IeumDocument& document)
{
    if (currentIndex < 0 || currentIndex >= static_cast<int>(commands.size()))
        return false;

    commands[static_cast<size_t>(currentIndex)]->undo(document);
    --currentIndex;
    return true;
}

bool IeumDocumentHistory::redo(IeumDocument& document)
{
    if (currentIndex + 1 >= static_cast<int>(commands.size()))
        return false;

    ++currentIndex;
    commands[static_cast<size_t>(currentIndex)]->execute(document);
    return true;
}

void IeumDocumentHistory::clear()
{
    commands.clear();
    currentIndex = -1;
}

std::unique_ptr<IeumCommand> createAddBindingCommand(const IeumBindingSpec& spec)
{
    return std::make_unique<AddBindingCommand>(spec);
}

std::unique_ptr<IeumCommand> createRemoveBindingCommand(const BindingId& id)
{
    return std::make_unique<RemoveBindingCommand>(id);
}

std::unique_ptr<IeumCommand> createUpdateBindingCommand(const IeumBindingSpec& oldSpec, const IeumBindingSpec& newSpec)
{
    return std::make_unique<UpdateBindingCommand>(oldSpec, newSpec);
}

} // namespace Ieum
