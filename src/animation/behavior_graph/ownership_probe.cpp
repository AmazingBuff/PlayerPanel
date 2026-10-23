//
// Created by AmazingBuff on 2026/10/23.
//
#include "animation/behavior_graph/ownership_probe.h"

#include "RE/B/BShkbAnimationGraph.h"
#include "RE/H/hkbBehaviorGraph.h"
#include "RE/H/hkbCharacterSetup.h"

PLUGIN_NAMESPACE_BEGIN
namespace animation::behavior_graph
{
namespace
{
    std::uintptr_t identity(void const* address)
    {
        return reinterpret_cast<std::uintptr_t>(address);
    }
}

void observe_source_baseline(RE::BShkbAnimationGraph const& graph, std::uint32_t actor_id,
    std::uint64_t request, std::uint64_t source_generation)
{
    RE::hkbCharacter const& character = graph.characterInstance;
    RE::hkbCharacterSetup const* const setup = character.setup.get();
    RE::hkbBehaviorGraph const* const behavior = character.behaviorGraph.get();

    logger::info("BEHAVIOR GRAPH actor={:08x} request={} source-generation={} role=source-baseline candidate=none construction=unverified gate1=not-passed layout=commonlib-declared runtime-layout-verified=no snapshot=non-atomic null-address=0x0 uncollected=unverified",
        actor_id, request, source_generation);
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=container graph={:#x} holder={:#x} root={:#x} character={:#x} outer-behavior={:#x} project-db={:#x}",
        actor_id, request, identity(&graph), identity(graph.holder), identity(graph.rootNode), identity(&character),
        identity(graph.behaviorGraph), identity(graph.projectDBData));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=character setup={:#x} behavior={:#x} project={:#x} bindings={:#x} event-queue={:#x} world={:#x}",
        actor_id, request, identity(setup), identity(behavior), identity(character.projectData.get()),
        identity(character.animationBindingSet.get()), identity(character.eventQueue.get()), identity(character.world.get()));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=drivers ragdoll={:#x} controller={:#x} foot-ik={:#x} hand-ik={:#x} raycast={:#x} lod={} lod-tracks={}",
        actor_id, request, identity(character.ragdollDriver.get()), identity(character.characterControllerDriver.get()),
        identity(character.footIkDriver.get()), identity(character.handIkDriver.get()), identity(character.raycastInterface.get()),
        character.currentLOD, character.numTracksInLOD);
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=outputs world-from-model={:#x} pose-local={:#x} pose-count={} delete-world={} delete-pose={} generator0={:#x} generator1={:#x}",
        actor_id, request, identity(character.worldFromModel), identity(character.poseLocal), character.numPoseLocal,
        character.deleteWorldFromModel, character.deletePoseLocal, identity(graph.generatorOutputs[0]), identity(graph.generatorOutputs[1]));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=bones storage={:#x} entries={} animation-bones={} foot-ik={}",
        actor_id, request, identity(graph.boneNodes.data()), graph.boneNodes.size(), graph.numAnimBones, graph.doFootIK);
    if (setup)
    {
        logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=setup skeleton={:#x} unscaled-skeleton={:#x} bindings={:#x} data={:#x} mirrored-skeleton={:#x} critical-section={:#x}",
            actor_id, request, identity(setup->animationSkeleton.get()), identity(setup->unscaledAnimationSkeleton.get()),
            identity(setup->animationBindingSet.get()), identity(setup->data.get()), identity(setup->mirroredSkeleton.get()), identity(setup->criticalSection));
    }
    else
        logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=setup missing=true", actor_id, request);

    if (!behavior)
    {
        logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=runtime missing=true", actor_id, request);
        return;
    }
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=runtime root-generator={:#x} declared-root-clone={:#x} data={:#x} active-nodes={:#x} variable-values={:#x} variable-mode={}",
        actor_id, request, identity(behavior->rootGenerator.get()), identity(behavior->rootGeneratorClone.get()),
        identity(behavior->data.get()), identity(behavior->activeNodes), identity(behavior->variableValueSet.get()),
        static_cast<std::uint8_t>(behavior->variableMode.underlying()));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=maps template-to-clone={:#x} clone-to-template={:#x} listener-to-clone={:#x} active-template-to-index={:#x} active-child-indices={:#x} state-machine-templates={:#x}",
        actor_id, request, identity(behavior->nodeTemplateToCloneMap.get()), identity(behavior->nodeCloneToTemplateMap.get()),
        identity(behavior->stateListenerTemplateToCloneMap.get()), identity(behavior->activeNodeTemplateToIndexMap.get()),
        identity(behavior->activeNodesChildrenIndices.get()), identity(behavior->idToStateMachineTemplateMap.get()));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=transitions global-data={:#x} random={:#x} event-map={:#x} attribute-map={:#x} variable-map={:#x} property-map={:#x}",
        actor_id, request, identity(behavior->globalTransitionData.get()), identity(behavior->pseudoRandomGenerator.get()),
        identity(behavior->eventIDMap.get()), identity(behavior->attributeIDMap.get()), identity(behavior->variableIDMap.get()),
        identity(behavior->characterPropertyIDMap.get()));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=storage ids={:#x}/{} mirrored-ids={:#x}/{} jobs={:#x}/{} partitions={:#x}/{} partition-info={:#x}",
        actor_id, request, identity(behavior->uniqueIDPool.data()), behavior->uniqueIDPool.size(),
        identity(behavior->mirroredExternalIDMap.data()), behavior->mirroredExternalIDMap.size(),
        identity(behavior->jobs.data()), behavior->jobs.size(), identity(behavior->allPartitionMemory.data()),
        behavior->allPartitionMemory.size(), identity(behavior->nodePartitionInfo.get()));
    logger::info("BEHAVIOR GRAPH actor={:08x} request={} group=flags active={} linked={} update-active={} state-changed={} intermediate-outputs={} static-nodes={} next-id={}",
        actor_id, request, behavior->isActive, behavior->isLinked, behavior->updateActiveNodes,
        behavior->stateOrTransitionChanged, behavior->numIntermediateOutputs, behavior->numStaticNodes, behavior->nextUniqueID);
}
}
PLUGIN_NAMESPACE_END
