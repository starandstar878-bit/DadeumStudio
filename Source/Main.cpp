/*
  ==============================================================================

    This file contains the basic startup code for a JUCE application.

  ==============================================================================
*/

#include <JuceHeader.h>
#include "MainComponent.h"
#include "Gyeol/Export/JuceComponentExport.h"
#include "Gyeol/Runtime/PropertyBindingResolver.h"
#include "Gyeol/Widgets/WidgetRegistry.h"
#include <iostream>
#include <map>

namespace
{
    juce::StringArray parseCommandLineArgs(const juce::String& commandLine)
    {
        juce::StringArray args;
        args.addTokens(commandLine, true);
        args.trim();
        args.removeEmptyStrings();
        return args;
    }

    bool hasArg(const juce::StringArray& args, const juce::String& key)
    {
        for (const auto& arg : args)
        {
            if (arg == key)
                return true;
        }

        return false;
    }

    juce::String argValue(const juce::StringArray& args, const juce::String& prefix)
    {
        for (const auto& arg : args)
        {
            if (arg.startsWith(prefix))
                return arg.fromFirstOccurrenceOf(prefix, false, false).unquoted();
        }

        return {};
    }

    juce::int64 parseWidgetIdFromVar(const juce::var& value)
    {
        if (value.isInt() || value.isInt64())
            return static_cast<juce::int64>(value);

        return value.toString().trim().getLargeIntValue();
    }

    bool valueIsTruthy(const juce::var& value)
    {
        if (value.isBool())
            return static_cast<bool>(value);
        if (value.isInt() || value.isInt64() || value.isDouble())
            return std::abs(static_cast<double>(value)) > 0.000000000001;

        const auto text = value.toString().trim().toLowerCase();
        return text == "1" || text == "true" || text == "yes" || text == "on";
    }

    juce::String resolveRuntimeParamKey(const std::map<juce::String, juce::var>& params,
                                        const juce::String& requestedKey)
    {
        const auto trimmed = requestedKey.trim();
        if (trimmed.isEmpty())
            return {};

        if (params.find(trimmed) != params.end())
            return trimmed;

        for (const auto& entry : params)
        {
            if (entry.first.equalsIgnoreCase(trimmed))
                return entry.first;
        }

        return trimmed;
    }

    Gyeol::DocumentModel makePhase6SmokeDocument()
    {
        Gyeol::DocumentModel document;
        document.schemaVersion = Gyeol::currentSchemaVersion();

        Gyeol::WidgetModel button;
        button.id = 1001;
        button.type = Gyeol::WidgetType::button;
        button.bounds = { 24.0f, 20.0f, 110.0f, 32.0f };
        button.properties.set("text", juce::String("Apply"));

        Gyeol::WidgetModel slider;
        slider.id = 1002;
        slider.type = Gyeol::WidgetType::slider;
        slider.bounds = { 160.0f, 20.0f, 220.0f, 32.0f };
        slider.properties.set("slider.style", juce::String("linearHorizontal"));
        slider.properties.set("slider.rangeMin", 0.0);
        slider.properties.set("slider.rangeMax", 1.0);
        slider.properties.set("slider.step", 0.0);
        slider.properties.set("value", 0.25);
        slider.properties.set("minValue", 0.0);
        slider.properties.set("maxValue", 1.0);

        Gyeol::WidgetModel label;
        label.id = 1003;
        label.type = Gyeol::WidgetType::label;
        label.bounds = { 24.0f, 70.0f, 220.0f, 28.0f };
        label.properties.set("text", juce::String("Idle"));

        document.widgets.push_back(button);
        document.widgets.push_back(slider);
        document.widgets.push_back(label);

        Gyeol::LayerModel layer;
        layer.id = 5001;
        layer.name = "Layer 1";
        layer.order = 0;
        layer.visible = true;
        layer.locked = false;
        layer.memberWidgetIds = { button.id, slider.id, label.id };
        document.layers.push_back(layer);

        Gyeol::RuntimeParamModel paramA;
        paramA.key = "A";
        paramA.type = Gyeol::RuntimeParamValueType::number;
        paramA.defaultValue = 0.25;
        paramA.description = "Smoke number";
        paramA.exposed = true;

        Gyeol::RuntimeParamModel paramB;
        paramB.key = "B";
        paramB.type = Gyeol::RuntimeParamValueType::boolean;
        paramB.defaultValue = false;
        paramB.description = "Smoke toggle";
        paramB.exposed = true;

        document.runtimeParams.push_back(paramA);
        document.runtimeParams.push_back(paramB);

        Gyeol::PropertyBindingModel sliderBinding;
        sliderBinding.id = 2001;
        sliderBinding.name = "Slider from A";
        sliderBinding.enabled = true;
        sliderBinding.targetWidgetId = slider.id;
        sliderBinding.targetProperty = "value";
        sliderBinding.expression = "A";

        Gyeol::PropertyBindingModel sliderMinBinding;
        sliderMinBinding.id = 2002;
        sliderMinBinding.name = "Slider min from A";
        sliderMinBinding.enabled = true;
        sliderMinBinding.targetWidgetId = slider.id;
        sliderMinBinding.targetProperty = "minValue";
        sliderMinBinding.expression = "A * 0.5";

        document.propertyBindings.push_back(sliderBinding);
        document.propertyBindings.push_back(sliderMinBinding);

        Gyeol::RuntimeBindingModel clickBinding;
        clickBinding.id = 3001;
        clickBinding.name = "Button click -> update A and label";
        clickBinding.enabled = true;
        clickBinding.sourceWidgetId = button.id;
        clickBinding.eventKey = "onClick";

        Gyeol::RuntimeActionModel setParamAAction;
        setParamAAction.kind = Gyeol::RuntimeActionKind::setRuntimeParam;
        setParamAAction.paramKey = "A";
        setParamAAction.value = 0.9;
        clickBinding.actions.push_back(setParamAAction);

        Gyeol::RuntimeActionModel patchLabelTextAction;
        patchLabelTextAction.kind = Gyeol::RuntimeActionKind::setNodeProps;
        patchLabelTextAction.target.kind = Gyeol::NodeKind::widget;
        patchLabelTextAction.target.id = label.id;
        patchLabelTextAction.patch.set("text", juce::String("Clicked"));
        clickBinding.actions.push_back(patchLabelTextAction);

        Gyeol::RuntimeActionModel moveLabelAction;
        moveLabelAction.kind = Gyeol::RuntimeActionKind::setNodeBounds;
        moveLabelAction.targetWidgetId = label.id;
        moveLabelAction.bounds = { 250.0f, 70.0f, 180.0f, 28.0f };
        clickBinding.actions.push_back(moveLabelAction);

        Gyeol::RuntimeBindingModel commitBinding;
        commitBinding.id = 3002;
        commitBinding.name = "Slider commit -> set A from payload";
        commitBinding.enabled = true;
        commitBinding.sourceWidgetId = slider.id;
        commitBinding.eventKey = "onValueCommit";

        Gyeol::RuntimeActionModel payloadToParamAction;
        payloadToParamAction.kind = Gyeol::RuntimeActionKind::setRuntimeParam;
        payloadToParamAction.paramKey = "A";
        commitBinding.actions.push_back(payloadToParamAction);

        document.runtimeBindings.push_back(clickBinding);
        document.runtimeBindings.push_back(commitBinding);
        return document;
    }

    Gyeol::DocumentModel makePhase6InvalidSmokeDocument()
    {
        auto document = makePhase6SmokeDocument();
        if (!document.propertyBindings.empty())
        {
            document.propertyBindings.front().expression = "A +";
        }
        else
        {
            Gyeol::PropertyBindingModel invalidBinding;
            invalidBinding.id = 9001;
            invalidBinding.name = "Invalid expression";
            invalidBinding.enabled = true;
            invalidBinding.targetWidgetId = 1002;
            invalidBinding.targetProperty = "value";
            invalidBinding.expression = "A +";
            document.propertyBindings.push_back(invalidBinding);
        }

        return document;
    }

    juce::Result runPhase6ExportSmoke(const juce::StringArray& args)
    {
        const auto outputArg = argValue(args, "--output-dir=");
        juce::File outputDirectory;
        if (outputArg.isNotEmpty())
        {
            outputDirectory = juce::File(outputArg);
        }
        else
        {
            outputDirectory = juce::File::getCurrentWorkingDirectory()
                                .getChildFile("Builds")
                                .getChildFile("GyeolExport")
                                .getChildFile("Phase6Smoke_" + juce::String(juce::Time::currentTimeMillis()));
        }

        const auto document = makePhase6SmokeDocument();
        const auto registry = Gyeol::Widgets::makeDefaultWidgetRegistry();

        Gyeol::Export::ExportOptions options;
        options.outputDirectory = outputDirectory;
        options.projectRootDirectory = juce::File::getCurrentWorkingDirectory();
        options.componentClassName = "GyeolPhase6SmokeComponent";
        options.overwriteExistingFiles = true;
        options.writeManifestJson = true;
        options.writeRuntimeDataJson = true;

        Gyeol::Export::ExportReport report;
        const auto result = Gyeol::Export::exportToJuceComponent(document, registry, options, report);
        std::cout << report.toText() << std::endl;

        if (result.failed())
            return result;

        if (!report.generatedHeaderFile.existsAsFile()
            || !report.generatedSourceFile.existsAsFile()
            || !report.manifestFile.existsAsFile()
            || !report.runtimeDataFile.existsAsFile())
        {
            return juce::Result::fail("Smoke export output missing expected files.");
        }

        const auto runtimeJson = juce::JSON::parse(report.runtimeDataFile);
        auto* runtimeRoot = runtimeJson.getDynamicObject();
        if (runtimeRoot == nullptr)
            return juce::Result::fail("Smoke runtime JSON parse failed.");

        const auto* runtimeParamArray = runtimeRoot->getProperty("runtimeParams").getArray();
        const auto* propertyBindingArray = runtimeRoot->getProperty("propertyBindings").getArray();
        const auto* runtimeBindingArray = runtimeRoot->getProperty("runtimeBindings").getArray();
        if (runtimeParamArray == nullptr || runtimeParamArray->isEmpty())
            return juce::Result::fail("Smoke runtime JSON has no runtimeParams.");
        if (propertyBindingArray == nullptr || propertyBindingArray->isEmpty())
            return juce::Result::fail("Smoke runtime JSON has no propertyBindings.");
        if (runtimeBindingArray == nullptr || runtimeBindingArray->isEmpty())
            return juce::Result::fail("Smoke runtime JSON has no runtimeBindings.");

        // Scenario smoke: button(onClick) -> setRuntimeParam(A) -> propertyBinding(slider.value=A).
        std::map<juce::String, juce::var> simulatedParams;
        for (const auto& paramVar : *runtimeParamArray)
        {
            auto* paramObject = paramVar.getDynamicObject();
            if (paramObject == nullptr)
                continue;

            const auto key = paramObject->getProperty("key").toString().trim();
            if (key.isEmpty())
                continue;
            simulatedParams[key] = paramObject->getProperty("defaultValue");
        }

        bool clickBindingExecuted = false;
        for (const auto& bindingVar : *runtimeBindingArray)
        {
            auto* bindingObject = bindingVar.getDynamicObject();
            if (bindingObject == nullptr)
                continue;
            if (bindingObject->hasProperty("enabled")
                && !valueIsTruthy(bindingObject->getProperty("enabled")))
            {
                continue;
            }

            if (parseWidgetIdFromVar(bindingObject->getProperty("sourceWidgetId")) != 1001)
                continue;
            if (bindingObject->getProperty("eventKey").toString().trim() != "onClick")
                continue;

            auto* actions = bindingObject->getProperty("actions").getArray();
            if (actions == nullptr)
                continue;

            clickBindingExecuted = true;
            for (const auto& actionVar : *actions)
            {
                auto* action = actionVar.getDynamicObject();
                if (action == nullptr)
                    continue;

                const auto kind = action->getProperty("kind").toString().trim().toLowerCase();
                if (kind == "setruntimeparam")
                {
                    const auto requestedKey = action->getProperty("paramKey").toString().trim();
                    if (requestedKey.isEmpty())
                        continue;

                    const auto resolvedKey = resolveRuntimeParamKey(simulatedParams, requestedKey);
                    simulatedParams[resolvedKey] = action->getProperty("value");
                }
                else if (kind == "adjustruntimeparam")
                {
                    const auto requestedKey = action->getProperty("paramKey").toString().trim();
                    if (requestedKey.isEmpty())
                        continue;

                    const auto resolvedKey = resolveRuntimeParamKey(simulatedParams, requestedKey);
                    const auto delta = static_cast<double>(action->getProperty("delta"));
                    const auto current = simulatedParams.count(resolvedKey) > 0
                                           ? static_cast<double>(simulatedParams[resolvedKey])
                                           : 0.0;
                    simulatedParams[resolvedKey] = current + delta;
                }
                else if (kind == "toggleruntimeparam")
                {
                    const auto requestedKey = action->getProperty("paramKey").toString().trim();
                    if (requestedKey.isEmpty())
                        continue;

                    const auto resolvedKey = resolveRuntimeParamKey(simulatedParams, requestedKey);
                    const auto current = simulatedParams.count(resolvedKey) > 0
                                           ? valueIsTruthy(simulatedParams[resolvedKey])
                                           : false;
                    simulatedParams[resolvedKey] = !current;
                }
            }
        }

        if (!clickBindingExecuted)
            return juce::Result::fail("Smoke scenario missing onClick binding execution path.");

        bool sliderBindingEvaluated = false;
        for (const auto& bindingVar : *propertyBindingArray)
        {
            auto* bindingObject = bindingVar.getDynamicObject();
            if (bindingObject == nullptr)
                continue;
            if (bindingObject->hasProperty("enabled")
                && !valueIsTruthy(bindingObject->getProperty("enabled")))
            {
                continue;
            }

            if (parseWidgetIdFromVar(bindingObject->getProperty("targetWidgetId")) != 1002)
                continue;
            if (bindingObject->getProperty("targetProperty").toString().trim() != "value")
                continue;

            const auto expression = bindingObject->getProperty("expression").toString();
            const auto evaluation = Gyeol::Runtime::PropertyBindingResolver::evaluateExpression(expression,
                                                                                                simulatedParams);
            if (!evaluation.success)
                return juce::Result::fail("Smoke scenario property binding evaluation failed: " + evaluation.error);

            if (std::abs(evaluation.value - 0.9) > 0.0001)
            {
                return juce::Result::fail("Smoke scenario expected slider value 0.9 after onClick, got "
                                          + juce::String(evaluation.value, 6));
            }

            sliderBindingEvaluated = true;
            break;
        }

        if (!sliderBindingEvaluated)
            return juce::Result::fail("Smoke scenario missing slider.value property binding.");

        const auto sourceText = report.generatedSourceFile.loadFileAsString();
        if (!sourceText.contains("initializeRuntimeBridge()")
            || !sourceText.contains("dispatchRuntimeEvent(")
            || !sourceText.contains("applyPropertyBindings()")
            || !sourceText.contains("applyRuntimeAction("))
        {
            return juce::Result::fail("Smoke generated source missing runtime bridge functions.");
        }

        // Error-scenario smoke: malformed expression should fail export gracefully.
        const auto invalidDocument = makePhase6InvalidSmokeDocument();
        auto invalidOptions = options;
        invalidOptions.outputDirectory = outputDirectory.getSiblingFile(outputDirectory.getFileName() + "_invalid");

        Gyeol::Export::ExportReport invalidReport;
        const auto invalidResult = Gyeol::Export::exportToJuceComponent(invalidDocument,
                                                                        registry,
                                                                        invalidOptions,
                                                                        invalidReport);
        if (invalidResult.wasOk() || invalidReport.errorCount <= 0)
            return juce::Result::fail("Smoke invalid export must fail with validation errors.");

        std::cout << "Phase6 smoke export directory: " << outputDirectory.getFullPathName() << std::endl;
        std::cout << "Phase6 smoke checks: PASS" << std::endl;
        return juce::Result::ok();
    }
}

//==============================================================================
class DadeumStudioApplication  : public juce::JUCEApplication
{
public:
    //==============================================================================
    DadeumStudioApplication() {}

    const juce::String getApplicationName() override       { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override    { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    //==============================================================================
    void initialise (const juce::String& commandLine) override
    {
        const auto args = parseCommandLineArgs(commandLine);
        if (hasArg(args, "--phase6-export-smoke"))
        {
            const auto smokeResult = runPhase6ExportSmoke(args);
            if (smokeResult.failed())
            {
                std::cerr << "Phase6 smoke failed: " << smokeResult.getErrorMessage() << std::endl;
                setApplicationReturnValue(1);
            }
            else
            {
                setApplicationReturnValue(0);
            }

            quit();
            return;
        }

        // This method is where you should put your application's initialisation code..
        mainWindow.reset (new MainWindow (getApplicationName()));
    }

    void shutdown() override
    {
        // Add your application's shutdown code here..

        mainWindow = nullptr; // (deletes our window)
    }

    //==============================================================================
    void systemRequestedQuit() override
    {
        // This is called when the app is being asked to quit: you can ignore this
        // request and let the app carry on running, or call quit() to allow the app to close.
        quit();
    }

    void anotherInstanceStarted (const juce::String& commandLine) override
    {
        // When another instance of the app is launched while this one is running,
        // this method is invoked, and the commandLine parameter tells you what
        // the other instance's command-line arguments were.
        juce::ignoreUnused(commandLine);
    }

    //==============================================================================
    /*
        This class implements the desktop window that contains an instance of
        our MainComponent class.
    */
    class MainWindow    : public juce::DocumentWindow
    {
    public:
        MainWindow (juce::String name)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);

           #if JUCE_IOS || JUCE_ANDROID
            setFullScreen (true);
           #else
            setResizable (true, true);
            if (const auto* display = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay())
                windowedBounds = display->userArea.reduced (display->userArea.getWidth() / 10,
                                                            display->userArea.getHeight() / 10);
            else
                windowedBounds = { 120, 80, 1440, 900 };

            setBounds (windowedBounds);
            enterFullscreenMode();
           #endif

            setVisible (true);
        }

        bool keyPressed (const juce::KeyPress& key) override
        {
            if (key.getKeyCode() == juce::KeyPress::F11Key)
            {
                toggleFullscreenMode();
                return true;
            }

            if (key.getKeyCode() == juce::KeyPress::escapeKey && fullscreenMode)
            {
                exitFullscreenMode();
                return true;
            }

            return DocumentWindow::keyPressed(key);
        }

        void closeButtonPressed() override
        {
            // This is called when the user tries to close this window. Here, we'll just
            // ask the app to quit when this happens, but you can change this to do
            // whatever you need.
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

        /* Note: Be careful if you override any DocumentWindow methods - the base
           class uses a lot of them, so by overriding you might break its functionality.
           It's best to do all your work in your content component instead, but if
           you really have to override any DocumentWindow methods, make sure your
           subclass also calls the superclass's method.
        */

    private:
        void toggleFullscreenMode()
        {
            if (fullscreenMode)
                exitFullscreenMode();
            else
                enterFullscreenMode();
        }

        void enterFullscreenMode()
        {
            if (fullscreenMode)
                return;

            windowedBounds = getBounds();
            setUsingNativeTitleBar (false);
            setTitleBarHeight (0);
            setFullScreen (true);
            fullscreenMode = true;
        }

        void exitFullscreenMode()
        {
            if (! fullscreenMode)
                return;

            setFullScreen (false);
            setUsingNativeTitleBar (true);

            if (! windowedBounds.isEmpty())
                setBounds (windowedBounds);

            fullscreenMode = false;
        }

        bool fullscreenMode = false;
        juce::Rectangle<int> windowedBounds;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

private:
    std::unique_ptr<MainWindow> mainWindow;
};

//==============================================================================
// This macro generates the main() routine that launches the app.
START_JUCE_APPLICATION (DadeumStudioApplication)
