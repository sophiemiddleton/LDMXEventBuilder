// DataAggregator.hh
#ifndef DATAAGGREGATOR_H
#define DATAAGGREGATOR_H

class DataAggregator {
public:
    DataAggregator(EventMerger& merger_ref) : m_merger(merger_ref) {}

    void aggregate(PhysicsEventData& event) { // Pass by ref now for efficiency
        std::cout << "[Aggregator] Forwarding Event ID " << event.event_id << " to Merger." << std::endl;
        m_merger.merge_event(std::move(event));
    }
private:
    EventMerger& m_merger;
};
#endif
