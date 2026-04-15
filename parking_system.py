"""Campus parking availability index.

Implements an efficient area-based search for available parking spots.
"""

from __future__ import annotations

from dataclasses import dataclass
import heapq
from typing import Dict, List, Set, Tuple


@dataclass(frozen=True)
class ParkingSpot:
    """Represents one parking spot on campus."""

    spot_id: str
    area: str
    row: int
    col: int
    priority_score: int


class CampusParkingIndex:
    """Indexes parking spots by area for fast availability lookup.

    Strategy:
    - Keep a min-heap per area keyed by (priority_score, spot_id).
    - Keep an occupied set for O(1) occupancy checks.
    - Use lazy deletion: stale heap entries are skipped while querying.
    """

    def __init__(self, spots: List[ParkingSpot]) -> None:
        self.spot_by_id: Dict[str, ParkingSpot] = {s.spot_id: s for s in spots}
        self.available_heap_by_area: Dict[str, List[Tuple[int, str]]] = {}
        self.occupied: Set[str] = set()

        for spot in spots:
            self.available_heap_by_area.setdefault(spot.area, [])
            heapq.heappush(
                self.available_heap_by_area[spot.area],
                (spot.priority_score, spot.spot_id),
            )

    def occupy(self, spot_id: str) -> bool:
        """Marks a spot as occupied. Returns True if state changed."""
        if spot_id not in self.spot_by_id or spot_id in self.occupied:
            return False
        self.occupied.add(spot_id)
        return True

    def release(self, spot_id: str) -> bool:
        """Marks a spot as available. Returns True if state changed."""
        if spot_id not in self.spot_by_id or spot_id not in self.occupied:
            return False

        self.occupied.remove(spot_id)
        spot = self.spot_by_id[spot_id]
        heapq.heappush(
            self.available_heap_by_area[spot.area],
            (spot.priority_score, spot.spot_id),
        )
        return True

    def get_available_spots(self, area: str, k: int = 1) -> List[str]:
        """Returns up to k currently available spots in one area.

        Spots are returned by ascending priority_score and then spot_id.
        """
        heap = self.available_heap_by_area.get(area)
        if not heap or k <= 0:
            return []

        temp: List[Tuple[int, str]] = []
        result: List[str] = []

        while heap and len(result) < k:
            score, spot_id = heapq.heappop(heap)
            if spot_id in self.occupied:
                continue

            # Valid available entry
            result.append(spot_id)
            temp.append((score, spot_id))

        # Push valid items back so future queries remain correct.
        for entry in temp:
            heapq.heappush(heap, entry)

        return result


if __name__ == "__main__":
    # Quick sanity demo
    seed_spots = [
        ParkingSpot("A-01", "North", 1, 1, 2),
        ParkingSpot("A-02", "North", 1, 2, 1),
        ParkingSpot("B-01", "South", 1, 1, 1),
    ]
    index = CampusParkingIndex(seed_spots)
    print(index.get_available_spots("North", 2))
    index.occupy("A-02")
    print(index.get_available_spots("North", 2))
