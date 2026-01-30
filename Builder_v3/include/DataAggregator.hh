// DataAggregator.hh
#ifndef DATAAGGREGATOR_H
#define DATAAGGREGATOR_H

class DataAggregator {
public:
    DataAggregator(EventMerger& merger_ref) : m_merger(merger_ref) {}

    // CHANGE: Accept an rvalue reference (&&) to allow std::move to bind
    void aggregate(PhysicsEventData&& event) {
        // In a real system, this might send data across processes/network to the merger farm
        m_merger.merge_event(std::move(event));
    }
private:
    EventMerger& m_merger;
};
#endif
