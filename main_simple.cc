#include <iostream>
#include "EventBuilder.hh"
#include "DataAggregator.hh"
#include "PhysicsEventData.hh"
#include <vector>


// This function simulates gathering data from different sources
PhysicsEventData gather_data_for_time_window(long long timestamp) {
    PhysicsEventData event_data;
    event_data.timestamp = timestamp;
    event_data.tracker_info.timestamp = timestamp;
    event_data.hcal_info.timestamp = timestamp;

    // ... populate with data

    return event_data;
}

int main() {
    // Create the contributor for the combined system
    DataAggregator aggregator("LDMX-experiment", "combined-detector");

    long long event_timestamp = 1730000000;

    // Gather the data for that time window from the different sub-systems
    PhysicsEventData combined_payload = gather_data_for_time_window(event_timestamp);

    // CORRECT USAGE: Explicitly provide both template arguments to EventBuilder
    EventBuilder<PhysicsEventData, DataAggregator> builder(aggregator);
    auto combined_event = builder
        .with_payload(std::move(combined_payload))
        .build();

    // The build() method will call the correct constructor for GenericEvent<PhysicsEventData, DataAggregator>
    // std::make_unique<GenericEvent<PhysicsEventData, DataAggregator>>(aggregator, std::move(combined_payload));

    // Process the combined event
    std::cout << "Event Type: " << combined_event->type() << std::endl;
    std::cout << "System ID: " << combined_event->get_contributor().get_system_id() << std::endl;
    std::cout << "Event Timestamp: " << combined_event->get_payload().timestamp << std::endl;

    return 0;
}
