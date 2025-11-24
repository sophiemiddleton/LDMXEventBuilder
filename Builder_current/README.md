# Introduction

# Purpose

A simple starting point for the LDMX Event builder

# Compile

To run with cmake:

```
mkdir build
cd build
cmake ..
cd ..
cmake --build build
```
# Concept

Summary: The journey of a real-world event:

* Detector Signal: A particle interaction creates analog signals in the detector.
* Digitization: Specialized electronics convert the analog signals into digital data.
* Serialization: On-detector firmware and ASICs format the digital data into a standardized binary protocol, adding timestamps and event IDs. This creates the serialized "data fragment."
* Transmission: The fragments are sent over high-speed networks to a central data acquisition system.
* Buffering and Synchronization: The central system receives fragments from all sub-detectors, buffers them, and waits for a complete set belonging to the same event ID.
* Deserialization: The raw byte fragments are parsed and converted back into structured C++ objects.
* Event Building: The structured data from all sub-detectors is aggregated into a single, cohesive "event" object.
* Analysis: The final event object is then ready for further processing and analysis.

# This code

## `main.cc`:

This file demonstrates a complete, simulated workflow for a particle physics event builder. It orchestrates the process from generating simulated raw detector data to producing a final, combined event object ready for analysis:

    * Serialization: The main function simulates data from two detectors (Tracker and HCal). The `serialize_tracker_data` and `serialize_hcal_data` functions convert structured C++ data objects (`TrkData` and `HCalData`) into raw byte streams (std::vector<char>).
    * Fragment generation: The simulate_readout function simulates the readout electronics, creating `DataFragment` objects containing the serialized data. In a real system, this data would arrive over a high-speed network.
    * Collection: Two separate threads are used to simulate the parallel arrival of data fragments from the Tracker and HCal into the `FragmentBuffer`.
    * Aggregation: The `FragmentBuffer` collects and manages the incoming fragments. The `try_build_event` method checks if all fragments required for a given `event_id` have been received.
    * Deserialization and assembly: Once the event is complete, the assemble_payload function retrieves the fragments from the buffer. It then calls the specialized `deserialize_tracker_data` and `deserialize_hcal_data` functions (within `BinaryDeserializer.hh`) to reconstruct the original `TrkData` and `HCalData` objects. The function combines these into a single `PhysicsEventData` struct.
    * Final event creation: An `EventBuilder` is used to construct the final `GenericEvent` object. This object wraps the assembled `PhysicsEventData` with metadata from a`DataAggregator` instance, which provides context about the event source.
    * Analysis and output: The assembled event is then accessed for a simple report, printing details like the event ID, timestamp, and the number of hits from each detector.

## `EventBuilder.hh`:

Defines a generic EventBuilder class that encapsulates the construction of a `GenericEvent` object. It takes the deserialized payload and other contributing information and combines them to produce a complete, coherent event object.

`GenericEvent` constructed from:

* `Payload`: A generic type representing the data collected from the various detector subsystems for a single event. In the main.cpp example, this is PhysicsEventData.
* `ContributorType`: A type representing the source or category of the event data.


The `EventBuilder` then takes this deserialized Payload and assembles it into the final `GenericEvent` object, bundling the event data with its metadata (ContributorType). This makes the data ready for downstream processing, such as analysis or filtering by a High-Level Trigger.

## `Contributor.hh`

`Contributor.hh`, defines an abstract base class named `Contributor`. This class is a key part of the program's object-oriented design, establishing a common interface for any object that can be a source of event data.

## `DataAggregator.hh`

The `DataAggregator.hh` header file defines a simple class, `DataAggregator`, which is designed to represent a source of event data. It inherits from a `Contributor` class, making it a concrete implementation of that interface.


## `PhysicsEventData.hh`

`PhysicsEventData.hh` defines a C++ struct that serves as the central data payload for a fully assembled particle physics event. It combines information collected from multiple subsystems into a single, cohesive data structure, ready for further processing and analysis.

In the `main.cc` process the `assemble_payload` function uses the `PhysicsEventData` struct to combine the deserialized `TrkData` and `HCalData` from the raw fragments.

## `FragmentBuffer.hh`

The `FragmentBuffer.hh` header defines a class critical for the event building process, acting as a temporary storage and management area for raw data fragments from detector subsystems before they are combined into a complete event. It is responsible for storing fragments, grouping them by `event_id`, checking for event completeness, supplying fragments for processing, and managing memory and potential timeouts. Core components likely include data structures (like `std::map` or `std::unordered_map`) for fragment storage, methods for adding and retrieving fragments, logic for detecting complete events (like `try_build_event`), and mechanisms for concurrency management such as mutexes and conditional variables.

## `Fragment.hh`

The `Fragment.hh` header file defines the fundamental data structures used in the particle physics event building process. It formalizes what a "data fragment" is by breaking it into a structured header containing metadata and a raw payload of data.

The `FragmentHeader` struct contains all the essential metadata needed to manage a data fragment.

    * `timestamp`: The time recorded by the Level-1 (L1) trigger when the particle collision was accepted. This timestamp is vital for correlating fragments, especially if they arrive out of order.
    * `event_id`: A unique identifier assigned by the trigger system to the specific collision. The event builder uses this ID to group fragments that belong to the same event.
    * `contributor_id`: An identifier specifying the source of the data, as defined by the ContributorId enum.
    subsystem_id: The identifier for the detector subsystem, as defined by the SubsystemId enum.
    * `data_size`: The size, in bytes, of the raw data payload. This is important for networking and memory management, as it tells the receiving system how much data to expect for this fragment.

The `DataFragment` struct combines the metadata and the raw data payload into a single, logical unit.

    * `header`: The `FragmentHeader` instance associated with this fragment.
    * `payload`: A `std::vector<char>` containing the raw byte data from the detector's readout electronics. This data is in a serialized format, meaning it's packed as a stream of bytes and needs to be deserialized later by the `assemble_payload` function.


## `BinaryDeserializer.hh`

The `BinaryDeserializer.hh` header defines a class, `BinaryDeserializer`, along with specific functions to deserialize binary data payloads from detector subsystems. This process, which is the inverse of serialization, reads raw byte streams and reconstructs the original structured C++ data objects

This class provides a set of tools to read and unpack data from a byte buffer. It maintains an internal position (`m_pos`) to track where it is in the buffer, allowing for sequential reading.

For each system there is a deserialize function. These create a `BinaryDeserializer` instance for the detectors raw payload. It then explicitly reads the data in the order it was serialized.

# The Simulation

Within the `main.cc` file we create two threads: a simulator and a builder.

The simulation thread acts to simulate the arrival of multiple data contributions, representing a real physics "event".

Three detectors are assumed:

* Hcal
* Ecal
* tracker

The exact number of fragments from each detector is randomly chosen on a per event basis. This is meant to add some realism. The exact structure of the data for each system is defined by their respective header files.

An example `*Frame.hh` struct is built added, this is taken from an example of the ECON-D data-packet. Eventually more work is needed here.

A time-stamp is assigned to the event and the time at the tracker, hcal, ecal has some random clock jitter added on to provide more realism.

For each detector a call to the `simulate_tcp_client(SubsystemId id, unsigned int event_id, long long timestamp, std::vector<char> serialized_payload, int port)` function is then made. This function simulates the passage of this fake data via a TCP bridge. Serialization takes place via the `serialize_*_data` functions. The size of the event is calculated this is assigned in memory. The FragementTrailer acts to perform a `checksum` ensuring all relevant fragments are passed.

Some latency is assigned using the `std::this_thread::sleep_for(...)` command.

# The Builder

On a second thread the event building takes place. Three values are defined:

```
const long long coherence_window_ns = 5529262;
const long long latency_delay_ns = 100;
const int min_subsystems_for_event = 3;
```

The `coherence_window_ns` describes the event window. The `latency_delay_ns` is an estimate of system lag and the `min_subsystems_for_event` is the expected number of systems contributing to the event.

Initially the builder asks the function `has_expired_fragments(reference_time, coherence_window_ns)` if there is a valid event fragment window.
