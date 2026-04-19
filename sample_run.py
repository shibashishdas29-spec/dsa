from parking_system import CampusParkingIndex, ParkingSpot


def build_demo_index() -> CampusParkingIndex:
    spots = [
        ParkingSpot("N-101", "North", 1, 1, 1),
        ParkingSpot("N-102", "North", 1, 2, 3),
        ParkingSpot("N-103", "North", 1, 3, 2),
        ParkingSpot("C-201", "Central", 2, 1, 1),
        ParkingSpot("C-202", "Central", 2, 2, 2),
        ParkingSpot("S-301", "South", 3, 1, 1),
    ]
    return CampusParkingIndex(spots)


def run_samples() -> None:
    idx = build_demo_index()

    print("Sample 1")
    print("Input : area=North, k=2")
    print("Output:", idx.get_available_spots("North", 2))
    print()

    print("Sample 2")
    idx.occupy("N-101")
    print("Input : occupy(N-101), area=North, k=2")
    print("Output:", idx.get_available_spots("North", 2))
    print()

    print("Sample 3")
    idx.occupy("N-103")
    print("Input : occupy(N-103), area=North, k=3")
    print("Output:", idx.get_available_spots("North", 3))
    print()

    print("Sample 4")
    idx.release("N-101")
    print("Input : release(N-101), area=North, k=2")
    print("Output:", idx.get_available_spots("North", 2))
    print()

    print("Sample 5")
    print("Input : area=Central, k=5")
    print("Output:", idx.get_available_spots("Central", 5))


if __name__ == "__main__":
    run_samples()
