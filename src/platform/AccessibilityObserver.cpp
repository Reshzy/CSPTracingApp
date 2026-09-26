#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include "platform/AccessibilityObserver.h"

#include <windows.h>

#include <objbase.h>
#include <oleauto.h>
#include <UIAutomationClient.h>
#include <wrl/client.h>

#include <cctype>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <memory>
#include <utility>

namespace tracing::platform {
namespace {

using Microsoft::WRL::ComPtr;

// Documented UIA HRESULT; UIAutomationClient.h does not always declare the macro.
constexpr HRESULT kUiaElementNotAvailable = static_cast<HRESULT>(0x80040201L);

struct BstrDeleter
{
    void operator()(BSTR value) const noexcept
    {
        if (value != nullptr)
        {
            SysFreeString(value);
        }
    }
};

using UniqueBstr = std::unique_ptr<OLECHAR, BstrDeleter>;

bool IsFiniteValue(double value) noexcept
{
    return std::isfinite(value);
}

void AppendAsciiLower(std::string& out, std::string const& text)
{
    out.reserve(out.size() + text.size());
    for (unsigned char ch : text)
    {
        out.push_back(static_cast<char>(std::tolower(ch)));
    }
}

bool ContainsAsciiInsensitive(std::string const& haystack, char const* needle)
{
    if (needle == nullptr || needle[0] == '\0')
    {
        return false;
    }
    std::string lowered;
    AppendAsciiLower(lowered, haystack);
    return lowered.find(needle) != std::string::npos;
}

bool ContainsUtf8(std::string const& haystack, char const* needle)
{
    return needle != nullptr && haystack.find(needle) != std::string::npos;
}

std::string WideToUtf8(wchar_t const* text)
{
    if (text == nullptr || text[0] == L'\0')
    {
        return {};
    }
    int const bytes = WideCharToMultiByte(CP_UTF8, 0, text, -1, nullptr, 0, nullptr, nullptr);
    if (bytes <= 1)
    {
        return {};
    }
    std::string out(static_cast<std::size_t>(bytes - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, text, -1, out.data(), bytes, nullptr, nullptr);
    return out;
}

std::string BstrToUtf8(BSTR text)
{
    return WideToUtf8(text);
}

bool ParseFiniteDouble(std::string const& text, double& value) noexcept
{
    if (text.empty())
    {
        return false;
    }
    char* end = nullptr;
    value = std::strtod(text.c_str(), &end);
    if (end == text.c_str())
    {
        return false;
    }
    while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
    {
        ++end;
    }
    if (end != nullptr && *end == '%')
    {
        ++end;
    }
    while (end != nullptr && *end != '\0' && std::isspace(static_cast<unsigned char>(*end)))
    {
        ++end;
    }
    return (end == nullptr || *end == '\0') && IsFiniteValue(value);
}

AccessibilityComponent ClassifyComponent(std::string const& name, std::string const& automationId)
{
    auto matches = [&](char const* ascii, char const* utf8A, char const* utf8B) {
        return ContainsAsciiInsensitive(name, ascii) ||
               ContainsAsciiInsensitive(automationId, ascii) ||
               (utf8A != nullptr && (ContainsUtf8(name, utf8A) || ContainsUtf8(automationId, utf8A))) ||
               (utf8B != nullptr && (ContainsUtf8(name, utf8B) || ContainsUtf8(automationId, utf8B)));
    };
    if (matches("zoom", "\xE3\x82\xBA\xE3\x83\xBC\xE3\x83\xA0", "\xE6\x8B\xA1\xE5\xA4\xA7") ||
        ContainsAsciiInsensitive(name, "magnif") ||
        ContainsAsciiInsensitive(automationId, "magnif") ||
        (ContainsAsciiInsensitive(name, "scale") && !ContainsAsciiInsensitive(name, "timescale")))
    {
        return AccessibilityComponent::Zoom;
    }
    if (matches("rotat", "\xE5\x9B\x9E\xE8\xBB\xA2", nullptr) ||
        ContainsAsciiInsensitive(name, "angle") ||
        ContainsAsciiInsensitive(automationId, "angle"))
    {
        return AccessibilityComponent::Rotation;
    }
    if (matches("navigator", "\xE3\x83\x8A\xE3\x83\x93\xE3\x82\xB2\xE3\x83\xBC\xE3\x82\xBF", nullptr) ||
        ContainsAsciiInsensitive(name, "navi") ||
        ContainsAsciiInsensitive(automationId, "navi") ||
        ContainsAsciiInsensitive(name, "overview") ||
        ContainsAsciiInsensitive(automationId, "overview"))
    {
        return AccessibilityComponent::Navigator;
    }
    return AccessibilityComponent::None;
}

bool NodeHasNumeric(AccessibilityNode const& node, double& numeric) noexcept
{
    if (node.hasRangeValue && node.rangeFinite)
    {
        numeric = node.rangeValue;
        return true;
    }
    if (node.hasValue && ParseFiniteDouble(node.valueText, numeric))
    {
        return true;
    }
    return false;
}

void FormatHresult(std::string& text, HRESULT hr)
{
    char buffer[16]{};
    std::snprintf(buffer, sizeof(buffer), "0x%08lX", static_cast<unsigned long>(hr));
    text = buffer;
}

HRESULT ReadElementNode(IUIAutomationElement* element, AccessibilityNode& node)
{
    node = AccessibilityNode{};
    if (element == nullptr)
    {
        node.available = false;
        return E_POINTER;
    }

    BSTR rawName = nullptr;
    HRESULT hr = element->get_CurrentName(&rawName);
    UniqueBstr name(rawName);
    if (FAILED(hr))
    {
        if (hr == static_cast<HRESULT>(kUiaElementNotAvailable))
        {
            node.available = false;
            return hr;
        }
    }
    else
    {
        node.name = BstrToUtf8(name.get());
    }

    BSTR rawId = nullptr;
    hr = element->get_CurrentAutomationId(&rawId);
    UniqueBstr automationId(rawId);
    if (SUCCEEDED(hr))
    {
        node.automationId = BstrToUtf8(automationId.get());
    }
    else if (hr == static_cast<HRESULT>(kUiaElementNotAvailable))
    {
        node.available = false;
        return hr;
    }

    BSTR rawType = nullptr;
    hr = element->get_CurrentLocalizedControlType(&rawType);
    UniqueBstr controlType(rawType);
    if (SUCCEEDED(hr))
    {
        node.controlType = BstrToUtf8(controlType.get());
    }

    ComPtr<IUnknown> rangeUnknown;
    hr = element->GetCurrentPattern(UIA_RangeValuePatternId, &rangeUnknown);
    if (hr == static_cast<HRESULT>(kUiaElementNotAvailable))
    {
        node.available = false;
        return hr;
    }
    if (SUCCEEDED(hr) && rangeUnknown)
    {
        ComPtr<IUIAutomationRangeValuePattern> range;
        if (SUCCEEDED(rangeUnknown.As(&range)) && range)
        {
            double value = 0.0;
            HRESULT const valueHr = range->get_CurrentValue(&value);
            if (valueHr == static_cast<HRESULT>(kUiaElementNotAvailable))
            {
                node.available = false;
                return valueHr;
            }
            node.hasRangeValue = true;
            if (SUCCEEDED(valueHr) && IsFiniteValue(value))
            {
                node.rangeFinite = true;
                node.rangeValue = value;
            }
        }
    }

    ComPtr<IUnknown> valueUnknown;
    hr = element->GetCurrentPattern(UIA_ValuePatternId, &valueUnknown);
    if (hr == static_cast<HRESULT>(kUiaElementNotAvailable))
    {
        node.available = false;
        return hr;
    }
    if (SUCCEEDED(hr) && valueUnknown)
    {
        ComPtr<IUIAutomationValuePattern> valuePattern;
        if (SUCCEEDED(valueUnknown.As(&valuePattern)) && valuePattern)
        {
            BSTR rawValue = nullptr;
            HRESULT const valueHr = valuePattern->get_CurrentValue(&rawValue);
            UniqueBstr valueText(rawValue);
            if (valueHr == static_cast<HRESULT>(kUiaElementNotAvailable))
            {
                node.available = false;
                return valueHr;
            }
            node.hasValue = true;
            if (SUCCEEDED(valueHr))
            {
                node.valueText = BstrToUtf8(valueText.get());
            }
        }
    }

    node.available = true;
    return S_OK;
}

class UiAutomationTreeSource final : public AccessibilityTreeSource
{
public:
    explicit UiAutomationTreeSource(HWND hwnd) noexcept
        : hwnd_(hwnd)
    {
    }

    bool Open(std::string& error) override
    {
        Close();
        if (hwnd_ == nullptr)
        {
            error = "ElementFromHandle: null HWND";
            return false;
        }

        HRESULT hr = CoCreateInstance(
            CLSID_CUIAutomation,
            nullptr,
            CLSCTX_INPROC_SERVER,
            IID_PPV_ARGS(&automation_));
        if (FAILED(hr) || !automation_)
        {
            FormatHresult(error, hr);
            error = "CoCreateInstance(CLSID_CUIAutomation) " + error;
            return false;
        }

        hr = automation_->ElementFromHandle(hwnd_, &root_);
        if (FAILED(hr) || !root_)
        {
            FormatHresult(error, hr);
            error = "IUIAutomation::ElementFromHandle " + error;
            Close();
            return false;
        }

        hr = automation_->get_RawViewWalker(&walker_);
        if (FAILED(hr) || !walker_)
        {
            FormatHresult(error, hr);
            error = "IUIAutomation::get_RawViewWalker " + error;
            Close();
            return false;
        }

        Frame rootFrame{};
        rootFrame.element = root_;
        stack_.push_back(std::move(rootFrame));
        opened_ = true;
        return true;
    }

    bool Next(AccessibilityNode& node) override
    {
        node = AccessibilityNode{};
        if (!opened_ || !walker_)
        {
            return false;
        }

        while (!stack_.empty())
        {
            Frame& frame = stack_.back();
            if (!frame.emitted)
            {
                HRESULT const hr = ReadElementNode(frame.element.Get(), node);
                frame.emitted = true;
                if (FAILED(hr) && !node.available)
                {
                    return true;
                }

                ComPtr<IUIAutomationElement> child;
                HRESULT const childHr = walker_->GetFirstChildElement(frame.element.Get(), &child);
                if (childHr == static_cast<HRESULT>(kUiaElementNotAvailable))
                {
                    node.available = false;
                    return true;
                }
                if (SUCCEEDED(childHr) && child)
                {
                    Frame childFrame{};
                    childFrame.element = std::move(child);
                    stack_.push_back(std::move(childFrame));
                }
                return true;
            }

            ComPtr<IUIAutomationElement> sibling;
            HRESULT const siblingHr =
                walker_->GetNextSiblingElement(frame.element.Get(), &sibling);
            stack_.pop_back();
            if (siblingHr == static_cast<HRESULT>(kUiaElementNotAvailable))
            {
                node.available = false;
                return true;
            }
            if (SUCCEEDED(siblingHr) && sibling)
            {
                Frame siblingFrame{};
                siblingFrame.element = std::move(sibling);
                stack_.push_back(std::move(siblingFrame));
            }
        }
        return false;
    }

    void Close() noexcept override
    {
        stack_.clear();
        walker_.Reset();
        root_.Reset();
        automation_.Reset();
        opened_ = false;
    }

private:
    struct Frame
    {
        ComPtr<IUIAutomationElement> element;
        bool emitted = false;
    };

    HWND hwnd_ = nullptr;
    ComPtr<IUIAutomation> automation_;
    ComPtr<IUIAutomationElement> root_;
    ComPtr<IUIAutomationTreeWalker> walker_;
    std::vector<Frame> stack_;
    bool opened_ = false;
};

} // namespace

char const* FormatAccessibilityCapability(AccessibilityCapability capability) noexcept
{
    switch (capability)
    {
    case AccessibilityCapability::Unprobed:
        return "unprobed";
    case AccessibilityCapability::Probing:
        return "probing";
    case AccessibilityCapability::Unsupported:
        return "unsupported";
    case AccessibilityCapability::Supported:
        return "supported";
    case AccessibilityCapability::Stale:
        return "stale";
    case AccessibilityCapability::ProviderError:
        return "provider-error";
    case AccessibilityCapability::TargetClosed:
        return "target-closed";
    }
    return "unknown";
}

char const* FormatAccessibilityReject(AccessibilityReject reject) noexcept
{
    switch (reject)
    {
    case AccessibilityReject::Ok:
        return "ok";
    case AccessibilityReject::Disabled:
        return "disabled";
    case AccessibilityReject::NoSession:
        return "no-session";
    case AccessibilityReject::Unsupported:
        return "unsupported";
    case AccessibilityReject::ProviderError:
        return "provider-error";
    case AccessibilityReject::TargetClosed:
        return "target-closed";
    case AccessibilityReject::Stale:
        return "stale";
    case AccessibilityReject::GenerationChanged:
        return "generation-changed";
    }
    return "unknown";
}

char const* FormatAccessibilityComponent(AccessibilityComponent component) noexcept
{
    switch (component)
    {
    case AccessibilityComponent::None:
        return "none";
    case AccessibilityComponent::Zoom:
        return "zoom";
    case AccessibilityComponent::Rotation:
        return "rotation";
    case AccessibilityComponent::Navigator:
        return "navigator";
    }
    return "unknown";
}

std::string FormatAccessibilityReport(AccessibilitySnapshot const& snapshot)
{
    std::string text = "uia=";
    text += FormatAccessibilityCapability(snapshot.capability);
    text += " reject=";
    text += FormatAccessibilityReject(snapshot.reject);
    text += " enabled=";
    text += snapshot.enabled ? "yes" : "no";
    text += " foundZoom=";
    text += snapshot.foundZoom ? "yes" : "no";
    text += " foundRot=";
    text += snapshot.foundRotation ? "yes" : "no";
    text += " foundNav=";
    text += snapshot.foundNavigator ? "yes" : "no";
    text += " nodes=";
    text += std::to_string(snapshot.nodesVisited);
    text += " truncated=";
    text += snapshot.truncated ? "yes" : "no";
    text += " locale=";
    text += snapshot.locale.empty() ? "-" : snapshot.locale;
    text += " observed=";
    if (snapshot.observed.empty())
    {
        text += "none";
    }
    else
    {
        for (std::size_t i = 0; i < snapshot.observed.size(); ++i)
        {
            if (i != 0)
            {
                text += ",";
            }
            AccessibilityObservedProperty const& property = snapshot.observed[i];
            text += FormatAccessibilityComponent(property.component);
            text += ":";
            std::string const label =
                !property.automationId.empty() ? property.automationId : property.name;
            if (label.size() > 24)
            {
                text.append(label, 0, 24);
            }
            else
            {
                text += label;
            }
            if (property.hasNumeric)
            {
                char buffer[32]{};
                std::snprintf(buffer, sizeof(buffer), "=%.4g", property.numeric);
                text += buffer;
            }
        }
    }
    if (!snapshot.providerError.empty())
    {
        text += " err=";
        text += snapshot.providerError;
    }
    return text;
}

AccessibilityObserver::AccessibilityObserver(AccessibilityInspectLimits limits)
    : limits_(limits)
{
    if (limits_.maxNodes <= 0)
    {
        limits_.maxNodes = kMaxAccessibilityNodes;
    }
    if (limits_.maxObserved <= 0)
    {
        limits_.maxObserved = kMaxAccessibilityObserved;
    }
}

AccessibilityObserver::~AccessibilityObserver()
{
    CancelAndJoin();
}

void AccessibilityObserver::AttachSession(
    std::uintptr_t hwnd,
    std::uint32_t pid,
    std::uint64_t generation) noexcept
{
    CancelAndJoin();
    sessionAttached_ = hwnd != 0 && generation != 0;
    sessionHwnd_ = hwnd;
    sessionPid_ = pid;
    sessionGeneration_ = generation;
    last_ = MakeDisabled(
        AccessibilityCapability::Unprobed,
        AccessibilityReject::Disabled,
        generation);
}

void AccessibilityObserver::Detach() noexcept
{
    CancelAndJoin();
    sessionAttached_ = false;
    sessionHwnd_ = 0;
    sessionPid_ = 0;
    sessionGeneration_ = 0;
    last_ = MakeDisabled(
        AccessibilityCapability::Unprobed,
        AccessibilityReject::Disabled,
        0);
}

void AccessibilityObserver::MarkTargetClosed() noexcept
{
    CancelAndJoin();
    last_ = MakeDisabled(
        AccessibilityCapability::TargetClosed,
        AccessibilityReject::TargetClosed,
        sessionGeneration_);
}

AccessibilitySnapshot AccessibilityObserver::Inspect(
    AccessibilityTreeSource& source,
    AccessibilityInspectInput const& input)
{
    last_ = BuildSnapshot(source, input, nullptr);
    return last_;
}

bool AccessibilityObserver::BeginProbe(std::uintptr_t notifyHwnd, unsigned notifyMessage)
{
    if (probing_.load(std::memory_order_acquire))
    {
        return false;
    }
    if (!sessionAttached_ || sessionHwnd_ == 0 || sessionGeneration_ == 0)
    {
        last_ = MakeDisabled(
            AccessibilityCapability::Unsupported,
            AccessibilityReject::NoSession,
            0);
        return false;
    }
    if (notifyHwnd == 0 || notifyMessage == 0)
    {
        last_ = MakeDisabled(
            AccessibilityCapability::ProviderError,
            AccessibilityReject::ProviderError,
            sessionGeneration_);
        last_.providerError = "BeginProbe missing notify HWND/message";
        return false;
    }

    CancelAndJoin();
    cancel_.store(false, std::memory_order_release);
    probing_.store(true, std::memory_order_release);
    last_.capability = AccessibilityCapability::Probing;
    last_.reject = AccessibilityReject::Disabled;
    last_.enabled = false;
    last_.observed.clear();
    last_.providerError.clear();
    last_.targetGeneration = sessionGeneration_;

    std::uintptr_t const hwnd = sessionHwnd_;
    std::uint32_t const pid = sessionPid_;
    std::uint64_t const generation = sessionGeneration_;
    worker_ = std::thread(
        &AccessibilityObserver::WorkerMain,
        this,
        hwnd,
        pid,
        generation,
        notifyHwnd,
        notifyMessage);
    return true;
}

bool AccessibilityObserver::ApplyCompletedProbe(std::uint64_t postedGeneration)
{
    AccessibilitySnapshot pending;
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        if (!pendingReady_)
        {
            return false;
        }
        pending = std::move(pending_);
        pendingReady_ = false;
    }
    probing_.store(false, std::memory_order_release);

    if (!sessionAttached_ || postedGeneration != sessionGeneration_ ||
        pending.targetGeneration != sessionGeneration_)
    {
        last_ = MakeDisabled(
            AccessibilityCapability::Stale,
            AccessibilityReject::GenerationChanged,
            sessionGeneration_);
        return false;
    }

    last_ = std::move(pending);
    return true;
}

AccessibilitySnapshot const& AccessibilityObserver::Last() const noexcept
{
    return last_;
}

bool AccessibilityObserver::Enabled() const noexcept
{
    return last_.enabled && sessionAttached_ &&
           last_.capability == AccessibilityCapability::Supported &&
           last_.targetGeneration == sessionGeneration_;
}

bool AccessibilityObserver::SessionAttached() const noexcept
{
    return sessionAttached_;
}

bool AccessibilityObserver::Probing() const noexcept
{
    return probing_.load(std::memory_order_acquire);
}

AccessibilitySnapshot AccessibilityObserver::BuildSnapshot(
    AccessibilityTreeSource& source,
    AccessibilityInspectInput const& input,
    std::atomic<bool> const* cancel) const
{
    if (!input.sessionAttached)
    {
        AccessibilitySnapshot snapshot = MakeDisabled(
            AccessibilityCapability::Unsupported,
            AccessibilityReject::NoSession,
            0);
        snapshot.locale = input.locale;
        return snapshot;
    }
    if (input.sampleGeneration != input.attachedGeneration)
    {
        AccessibilitySnapshot snapshot = MakeDisabled(
            AccessibilityCapability::Stale,
            AccessibilityReject::GenerationChanged,
            input.attachedGeneration);
        snapshot.locale = input.locale;
        return snapshot;
    }
    if (!input.targetAlive)
    {
        AccessibilitySnapshot snapshot = MakeDisabled(
            AccessibilityCapability::TargetClosed,
            AccessibilityReject::TargetClosed,
            input.attachedGeneration);
        snapshot.locale = input.locale;
        return snapshot;
    }

    std::string error;
    if (!source.Open(error))
    {
        source.Close();
        AccessibilitySnapshot snapshot = MakeDisabled(
            AccessibilityCapability::ProviderError,
            AccessibilityReject::ProviderError,
            input.attachedGeneration);
        snapshot.locale = input.locale;
        snapshot.providerError = error.empty() ? "tree source Open failed" : error;
        return snapshot;
    }

    AccessibilitySnapshot snapshot{};
    snapshot.capability = AccessibilityCapability::Unsupported;
    snapshot.reject = AccessibilityReject::Unsupported;
    snapshot.targetGeneration = input.attachedGeneration;
    snapshot.locale = input.locale;
    auto const started = std::chrono::steady_clock::now();

    AccessibilityNode node{};
    while (snapshot.nodesVisited < limits_.maxNodes)
    {
        if (cancel != nullptr && cancel->load(std::memory_order_acquire))
        {
            break;
        }
        if (limits_.timeout.count() > 0 &&
            (std::chrono::steady_clock::now() - started) >= limits_.timeout)
        {
            snapshot.truncated = true;
            break;
        }
        if (!source.Next(node))
        {
            break;
        }
        ++snapshot.nodesVisited;
        if (!node.available)
        {
            int const visited = snapshot.nodesVisited;
            bool const truncated = snapshot.truncated;
            source.Close();
            snapshot = MakeDisabled(
                AccessibilityCapability::Stale,
                AccessibilityReject::Stale,
                input.attachedGeneration);
            snapshot.locale = input.locale;
            snapshot.nodesVisited = visited;
            snapshot.truncated = truncated;
            return snapshot;
        }

        AccessibilityComponent const component =
            ClassifyComponent(node.name, node.automationId);
        double numeric = 0.0;
        bool const hasNumeric = NodeHasNumeric(node, numeric);
        if (component == AccessibilityComponent::None || !hasNumeric)
        {
            continue;
        }

        if (component == AccessibilityComponent::Zoom)
        {
            snapshot.foundZoom = true;
        }
        else if (component == AccessibilityComponent::Rotation)
        {
            snapshot.foundRotation = true;
        }
        else if (component == AccessibilityComponent::Navigator)
        {
            snapshot.foundNavigator = true;
        }

        if (static_cast<int>(snapshot.observed.size()) < limits_.maxObserved)
        {
            AccessibilityObservedProperty property{};
            property.component = component;
            property.name = node.name;
            property.automationId = node.automationId;
            property.controlType = node.controlType;
            property.hasNumeric = true;
            property.numeric = numeric;
            snapshot.observed.push_back(std::move(property));
        }
    }

    if (snapshot.nodesVisited >= limits_.maxNodes)
    {
        AccessibilityNode extra{};
        if (source.Next(extra))
        {
            snapshot.truncated = true;
        }
    }
    source.Close();

    if (snapshot.foundZoom || snapshot.foundRotation || snapshot.foundNavigator)
    {
        snapshot.capability = AccessibilityCapability::Supported;
        snapshot.reject = AccessibilityReject::Ok;
        snapshot.enabled = true;
    }
    return snapshot;
}

AccessibilitySnapshot AccessibilityObserver::MakeDisabled(
    AccessibilityCapability capability,
    AccessibilityReject reject,
    std::uint64_t generation) const
{
    AccessibilitySnapshot snapshot{};
    snapshot.capability = capability;
    snapshot.reject = reject;
    snapshot.targetGeneration = generation;
    snapshot.enabled = false;
    return snapshot;
}

void AccessibilityObserver::CancelAndJoin() noexcept
{
    cancel_.store(true, std::memory_order_release);
    if (worker_.joinable())
    {
        worker_.join();
    }
    probing_.store(false, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pendingReady_ = false;
        pending_ = AccessibilitySnapshot{};
    }
    cancel_.store(false, std::memory_order_release);
}

void AccessibilityObserver::WorkerMain(
    std::uintptr_t hwnd,
    std::uint32_t pid,
    std::uint64_t generation,
    std::uintptr_t notifyHwnd,
    unsigned notifyMessage)
{
    (void)pid;
    HRESULT const initHr = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
    bool const comInitialized = SUCCEEDED(initHr);

    AccessibilityInspectInput input{};
    input.attachedGeneration = generation;
    input.sampleGeneration = generation;
    input.sessionAttached = hwnd != 0 && generation != 0;
    HWND const window = reinterpret_cast<HWND>(hwnd);
    input.targetAlive = window != nullptr && IsWindow(window) != FALSE;
    wchar_t locale[LOCALE_NAME_MAX_LENGTH]{};
    if (GetUserDefaultLocaleName(locale, LOCALE_NAME_MAX_LENGTH) > 0)
    {
        input.locale = WideToUtf8(locale);
    }

    AccessibilitySnapshot snapshot;
    if (!comInitialized)
    {
        snapshot = MakeDisabled(
            AccessibilityCapability::ProviderError,
            AccessibilityReject::ProviderError,
            generation);
        FormatHresult(snapshot.providerError, initHr);
        snapshot.providerError = "CoInitializeEx " + snapshot.providerError;
        snapshot.locale = input.locale;
    }
    else
    {
        UiAutomationTreeSource source(window);
        snapshot = BuildSnapshot(source, input, &cancel_);
    }

    {
        std::lock_guard<std::mutex> lock(pendingMutex_);
        pending_ = std::move(snapshot);
        pendingReady_ = true;
    }

    if (notifyHwnd != 0 && notifyMessage != 0)
    {
        if (PostMessageW(
                reinterpret_cast<HWND>(notifyHwnd),
                notifyMessage,
                static_cast<WPARAM>(generation),
                0) == FALSE)
        {
            probing_.store(false, std::memory_order_release);
        }
    }
    else
    {
        probing_.store(false, std::memory_order_release);
    }

    if (comInitialized)
    {
        CoUninitialize();
    }
}

} // namespace tracing::platform
