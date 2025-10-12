# Introduction

# Purpose

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
