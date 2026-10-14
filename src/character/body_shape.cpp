//
// Created by AmazingBuff on 2026/10/14.
//

#include "character/body_shape.h"

#include "RE/A/Actor.h"
#include "RE/T/TESObjectREFR.h"

#include <cmath>

PLUGIN_NAMESPACE_BEGIN

namespace
{
    // ---------------------------------------------------------------------------------------------
    // skee64's interface-exchange protocol and `IBodyMorphInterface`, mirrored for interop.
    //
    // Provenance (read 2026-10-14, not copied): upstream `expired6978/SKSE64Plugins`, files
    // `skee64/IPluginInterface.h` and `skee64/BodyMorphInterface.h`, blob
    // 7f65913f4c86e105fca421c1da285805dcd90eac; provider on this machine is RaceMenu AE's
    // `skee64.dll`, whose `GetVersion()` reports 5 in OBody's own log ("BodyMorph Version 5").
    //
    // What matters for correctness is the VTABLE ORDER, so every virtual of the interface is
    // declared here in upstream order even though this plugin calls only four of them: the compiler
    // then derives the same slots. New virtuals upstream are appended after `AddMorphShapeCallback`,
    // so a later version keeps these slots - but the version is still checked before first use, and
    // an interface older than the mirrored v5 is refused rather than called.
    // ---------------------------------------------------------------------------------------------

    using skee_u32 = std::uint32_t;
    using skee_u64 = std::uint64_t;

    class IPluginInterface
    {
    public:
        virtual ~IPluginInterface() = default;

        virtual skee_u32 GetVersion() = 0;
        virtual void Revert() = 0;
    };

    class IInterfaceMap
    {
    public:
        virtual IPluginInterface* QueryInterface(char const* a_name) = 0;
        virtual bool AddInterface(char const* a_name, IPluginInterface* a_interface) = 0;
        virtual IPluginInterface* RemoveInterface(char const* a_name) = 0;
    };

    struct InterfaceExchangeMessage
    {
        enum : std::uint32_t
        {
            kExchangeInterface = 0x9E3779B9
        };

        IInterfaceMap* interfaceMap = nullptr;
    };

    class IBodyMorphInterface : public IPluginInterface
    {
    public:
        class MorphKeyVisitor
        {
        public:
            virtual void Visit(char const*, float) = 0;
        };

        class StringVisitor
        {
        public:
            virtual void Visit(char const*) = 0;
        };

        class ActorVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*) = 0;
        };

        class MorphValueVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*, char const*, char const*, float) = 0;
        };

        class MorphVisitor
        {
        public:
            virtual void Visit(RE::TESObjectREFR*, char const*) = 0;
        };

        virtual void SetMorph(RE::TESObjectREFR*, char const*, char const*, float) = 0;
        virtual float GetMorph(RE::TESObjectREFR*, char const*, char const*) = 0;
        virtual void ClearMorph(RE::TESObjectREFR*, char const*, char const*) = 0;
        virtual float GetBodyMorphs(RE::TESObjectREFR*, char const*) = 0;
        virtual void ClearBodyMorphNames(RE::TESObjectREFR*, char const*) = 0;
        virtual void VisitMorphs(RE::TESObjectREFR*, MorphVisitor&) = 0;
        virtual void VisitKeys(RE::TESObjectREFR*, char const*, MorphKeyVisitor&) = 0;
        virtual void VisitMorphValues(RE::TESObjectREFR*, MorphValueVisitor&) = 0;
        virtual void ClearMorphs(RE::TESObjectREFR*) = 0;
        virtual void ApplyVertexDiff(RE::TESObjectREFR*, RE::NiAVObject*, bool) = 0;
        virtual void ApplyBodyMorphs(RE::TESObjectREFR*, bool) = 0;
        virtual void UpdateModelWeight(RE::TESObjectREFR*, bool) = 0;
        virtual void SetCacheLimit(skee_u64) = 0;
        virtual bool HasMorphs(RE::TESObjectREFR*) = 0;
        virtual skee_u32 EvaluateBodyMorphs(RE::TESObjectREFR*) = 0;
        virtual bool HasBodyMorph(RE::TESObjectREFR*, char const*, char const*) = 0;
        virtual bool HasBodyMorphName(RE::TESObjectREFR*, char const*) = 0;
        virtual bool HasBodyMorphKey(RE::TESObjectREFR*, char const*) = 0;
        virtual void ClearBodyMorphKeys(RE::TESObjectREFR*, char const*) = 0;
        virtual void VisitStrings(StringVisitor&) = 0;
        virtual void VisitActors(ActorVisitor&) = 0;
        virtual skee_u64 ClearMorphCache() = 0;
        virtual void AddMorphShapeCallback(void (*)(RE::TESObjectREFR*, RE::NiAVObject*, RE::BSGeometry*, RE::NiSkinPartition*, RE::NiBinaryExtraData*), skee_u64) = 0;
    };

    // OBody's own morph key: every preset it applies lands under this key, and its "already handled"
    // marker is the morph `obody_processed` under the same key.
    constexpr char OBody_Morph_Key[] = "OBody";

    constexpr float Morph_Match_Epsilon = 0.0001f;

    IBodyMorphInterface* acquire()
    {
        static IBodyMorphInterface* s_morph = nullptr;
        if (s_morph)
            return s_morph;

        const SKSE::MessagingInterface* const messaging = SKSE::GetMessagingInterface();
        if (!messaging)
            return nullptr;

        // The interface is published on the plugin named "skee": a message of the exchange type
        // carries back the interface map, and the BodyMorph interface is fetched from it by name.
        // Failure is normal before skee64 has registered (and on a setup without RaceMenu), so this
        // is retried on every call and only reported once.
        InterfaceExchangeMessage message{};
        if (!messaging->Dispatch(InterfaceExchangeMessage::kExchangeInterface, std::addressof(message),
                sizeof(message), "skee") ||
            !message.interfaceMap)
        {
            static bool s_reported = false;
            if (!s_reported)
            {
                s_reported = true;
                logger::info("SCOPY SHAPE skee BodyMorph unavailable (no RaceMenu interface exchange); the puppet keeps whatever body it was given");
            }
            return nullptr;
        }

        IPluginInterface* const base = message.interfaceMap->QueryInterface("BodyMorph");
        if (!base)
            return nullptr;

        const skee_u32 version = base->GetVersion();
        if (version < 5)
        {
            logger::warn("SCOPY SHAPE skee BodyMorph version={} is older than the mirrored layout (5); the copy stays off", version);
            return nullptr;
        }

        s_morph = static_cast<IBodyMorphInterface*>(base);   // single inheritance: the base pointer is the object
        logger::info("SCOPY SHAPE skee BodyMorph interface acquired version={}", version);
        return s_morph;
    }

    // Compares the source's morphs against the target's without writing anything.
    class MorphComparer final : public IBodyMorphInterface::MorphValueVisitor
    {
    public:
        MorphComparer(RE::TESObjectREFR* a_target, IBodyMorphInterface* a_morph)
            : m_target(a_target), m_morph(a_morph)
        {
        }

        void Visit(RE::TESObjectREFR*, char const* a_name, char const* a_key, float a_value) override
        {
            if (!a_name || !*a_name)
                return;
            ++m_compared;
            const float current = m_morph->GetMorph(m_target, a_name, a_key ? a_key : "");
            if (std::abs(current - a_value) > Morph_Match_Epsilon)
                ++m_mismatched;
        }

        [[nodiscard]] std::uint32_t compared() const { return m_compared; }
        [[nodiscard]] std::uint32_t mismatched() const { return m_mismatched; }

    private:
        RE::TESObjectREFR* m_target;
        IBodyMorphInterface* m_morph;
        std::uint32_t m_compared = 0;
        std::uint32_t m_mismatched = 0;
    };

    class MorphCopier final : public IBodyMorphInterface::MorphValueVisitor
    {
    public:
        MorphCopier(RE::TESObjectREFR* a_target, IBodyMorphInterface* a_morph, BodyShapeSync& a_sync)
            : m_target(a_target), m_morph(a_morph), m_sync(a_sync)
        {
        }

        void Visit(RE::TESObjectREFR*, char const* a_name, char const* a_key, float a_value) override
        {
            if (!a_name || !*a_name)
                return;
            m_morph->SetMorph(m_target, a_name, a_key ? a_key : "", a_value);
            ++m_sync.copied;
            if (m_sync.sample.size() < 140)
            {
                if (!m_sync.sample.empty())
                    m_sync.sample += ",";
                m_sync.sample += fmt::format("{}={:.0f}", a_name, a_value);
            }
        }

    private:
        RE::TESObjectREFR* m_target;
        IBodyMorphInterface* m_morph;
        BodyShapeSync& m_sync;
    };
}

bool body_shape_available()
{
    return acquire() != nullptr;
}

BodyShapeSync body_shape_sync(RE::Actor& a_target, RE::Actor& a_source)
{
    BodyShapeSync sync;

    IBodyMorphInterface* const morph = acquire();
    if (!morph)
        return sync;
    sync.available = true;

    MorphComparer comparer(&a_target, morph);
    morph->VisitMorphValues(&a_source, comparer);
    sync.compared = comparer.compared();
    sync.mismatched = comparer.mismatched();

    // A figure that already carries the source's shape is left alone: re-applying identical morphs
    // would rewrite the vertex buffers for nothing, and the check is what keeps this callable on a
    // panel cadence.
    if (sync.compared == 0 || sync.mismatched == 0)
        return sync;

    // Clear the OBody key first: that is the random preset OBody assigned to this actor, and the
    // source's own values (which live under the same key) are written on top of the cleared set.
    morph->ClearBodyMorphKeys(&a_target, OBody_Morph_Key);

    MorphCopier copier(&a_target, morph, sync);
    morph->VisitMorphValues(&a_source, copier);

    // `deferUpdate = false`: the vertex buffers are rewritten inside this call, so the change is in
    // the geometry the next draw consumes rather than in some later skin update.
    morph->ApplyBodyMorphs(&a_target, false);
    sync.applied = true;
    return sync;
}

PLUGIN_NAMESPACE_END
