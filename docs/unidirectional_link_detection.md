# Unidirectional Link Detection

## Problem Statement

At marginal signal levels (SNR -11 to -14), radio links between LoRa nodes can become purely one-directional due to antenna orientation, physical obstructions, or hardware asymmetry. One direction works while the other is completely dead.

In the sf9_17Power_experiment with 15 nodes, node 0x3428 could receive routing table broadcasts from gateway 0x77A4 consistently (~every 39 seconds, RSSI 122-127), but 0x77A4's log contained zero received packets from 0x3428. Three nodes (0xDD58, 0x14A4, 0x3428) never appeared in MQTT monitoring because all their data was routed through this dead link.

## Root Cause

The library has the data to detect unidirectional links but two code locations prevented it from acting:

### Failure Chain

1. **Route change**: Node 0x3428 receives a routing table broadcast from 0x77A4, marking it as a direct neighbor (hop_count=1). This replaces the correct 2-hop route via=0x006C.

2. **Quality looks excellent**: Link quality to 0x77A4 calculates as 230/255 based on one-way EWMA reception. The library checks what 0x77A4 reports about 0x3428 via `GetLinkQualityFor(0x3428)`, which returns 0 (0x77A4 has never heard 0x3428). But `network_service.cpp` defaults 0 to 128, masking the unidirectional nature. Final quality: (230 + 128) / 2 = 179.

3. **Wrong path selected**: With quality 179 at 1 hop, the direct route is preferred over the working 2-hop route via 0x006C (quality ~90).

4. **TDMA slot mismatch**: Node 0x3428 allocates a DATA TX slot for sending to 0x77A4. But 0x77A4 doesn't know 0x3428 as a direct neighbor, so it allocates that slot as SLEEP (radio off).

5. **Data lost**: 0x3428 transmits on the DATA TX slot. Intermediate node 0x006C receives the packet but correctly ignores it (next_hop != its address). Gateway 0x77A4's radio is off. The data is lost.

6. **Repeats indefinitely**: All 27+ monitoring messages forwarded toward 0x77A4 via 0x3428 are lost.

### Bug Locations

**`network_service.cpp:213-214`** — Masks `GetLinkQualityFor()=0` by defaulting to 128:
```cpp
uint8_t local_link_quality = routing_msg.GetLinkQualityFor(node_address_);
if (local_link_quality == 0) {
    local_link_quality = 128;  // Masks unidirectional links
}
```

**`network_node_route.cpp:CalculateQuality()`** — No penalty when `remote_link_quality == 0`:
```cpp
if (remote_link_quality > 0) {
    return (ewma_quality + remote_link_quality) / 2;
}
return ewma_quality;  // Full quality even though peer can't hear us
```

## Detection Algorithm

### Mechanism

When node A receives a routing table from node B, A checks whether B lists A in its entries via `GetLinkQualityFor(A)`. If A is not listed, `GetLinkQualityFor()` returns 0, meaning "B doesn't hear A."

The fix lets this 0 propagate (instead of defaulting to 128) and adds a penalty after confirmation:

1. **Grace period**: For the first 2 routing table exchanges, `remote_link_quality=0` is tolerated. The peer hasn't had time to list us yet — bidirectional links typically get `remote_link_quality > 0` by superframe 2.

2. **Confirmation**: After `messages_expected >= 3` routing table exchanges where the peer still doesn't list us, the link is classified as confirmed unidirectional.

3. **Penalty**: Quality is reduced to `ewma_quality / 4`. With a typical ewma of ~200, this produces quality ~50.

### Why Threshold 3

Bidirectional link timeline:
- Superframe N: A discovers B (messages_expected=0)
- Superframe N+1: A broadcasts routing table listing B. B receives it, adds A.
- Superframe N+2: B broadcasts routing table listing A with quality > 0. A receives it.

So `remote_link_quality > 0` by superframe 2 for bidirectional links. Threshold 3 provides margin for one dropped packet.

### Why ewma/4

Route cost comparison (ETX-based: `cost = hop_count × 65536 / quality`):

| Route | Quality | Cost |
|-------|---------|------|
| 1-hop unidirectional, ewma=200, penalty=200/4=50 | 50 | 1×65536/50 = 1310 |
| 2-hop bidirectional relay, quality=200 | 200 | 2×65536/200 = 655 |

The 2-hop relay wins decisively. A factor of `ewma/2` would produce equal costs (indeterminate routing), so `ewma/4` is the minimum divisor that reliably causes route switching.

### Recovery

Recovery is automatic. Once the peer starts listing us (`remote_link_quality > 0`), the penalty branch is skipped entirely and quality returns to `(ewma + remote) / 2`. This handles transient unidirectionality (e.g., temporary interference) without permanent damage.

## Topology Example

```
Gateway(NM) <---> Relay <---> Edge
      |                         ^
      +--- unidirectional ------+
           (GW broadcasts reach Edge,
            Edge transmissions don't reach GW)
```

**Before fix**: Edge sees Gateway as 1-hop quality 179 → routes directly → data lost (TDMA mismatch).

**After fix**: After 3 superframes, Edge's quality to Gateway drops to ~57. The 2-hop route via Relay (quality ~90) becomes preferred. Data flows Edge→Relay→Gateway successfully.

## TDMA Impact

Without detection, the sending node allocates a DATA TX slot for a peer whose radio is off during that slot. The distributed TDMA algorithm requires both sides to agree on the neighbor relationship for slot alignment. A unidirectional link creates a one-sided neighbor relationship that breaks this assumption.

With detection, the route switches to the relay path where both hops have proper TDMA slot alignment (both sides recognize each other as direct neighbors).
