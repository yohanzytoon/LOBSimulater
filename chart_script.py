# Create the LOB Backtester system architecture diagram
diagram_code = """
flowchart TD
    %% Market Data Input
    MD[Market Data Events] 
    
    %% Core Components Layer
    subgraph Core ["Core Components Layer"]
        OB[OrderBook<br/>Price-Time Priority<br/>L2/L3 Data]
        SG[Signals<br/>Microstructure Analytics<br/>Order Imbalance<br/>Microprice]
        BT[Backtester<br/>Event-Driven Engine]
        PF[Portfolio<br/>Position Tracking<br/>P&L]
        MT[Metrics<br/>Performance Analytics]
    end
    
    %% Strategy Layer
    subgraph Strategy ["Strategy Layer"]
        SB[Strategy Base Class]
        MM[Market Maker Strategy]
        MOM[Momentum Strategy]
        SS[Signal-based Strategy]
    end
    
    %% Language Interfaces
    subgraph Interfaces ["Language Interfaces"]
        CPP[C++17 Core Implementation]
        PY[Python Bindings<br/>pybind11]
        NB[Research Notebooks]
    end
    
    %% Infrastructure
    subgraph Infrastructure ["Infrastructure"]
        CM[CMake Build System]
        GT[GoogleTest &<br/>Google Benchmark]
        CI[CI/CD<br/>GitHub Actions]
        DOC[Documentation<br/>Doxygen]
    end
    
    %% Data Flow Connections
    MD --> OB
    OB --> SG
    SG --> BT
    
    %% Strategies consume data and signals
    OB -.-> SB
    SG -.-> SB
    
    %% Strategy inheritance
    SB --> MM
    SB --> MOM  
    SB --> SS
    
    %% Strategy execution flow
    MM --> BT
    MOM --> BT
    SS --> BT
    
    %% Backtester to Portfolio
    BT --> PF
    PF --> MT
    
    %% Interface connections
    CPP -.-> Core
    CPP -.-> Strategy
    PY -.-> CPP
    NB -.-> PY
    
    %% Infrastructure connections
    CM -.-> CPP
    GT -.-> CPP
    CI -.-> CM
    DOC -.-> CPP
    
    %% Styling
    classDef coreStyle fill:#B3E5EC,stroke:#1FB8CD,stroke-width:2px
    classDef strategyStyle fill:#A5D6A7,stroke:#2E8B57,stroke-width:2px
    classDef interfaceStyle fill:#FFEB8A,stroke:#D2BA4C,stroke-width:2px
    classDef infraStyle fill:#E0E0E0,stroke:#757575,stroke-width:2px
    
    class OB,SG,BT,PF,MT coreStyle
    class SB,MM,MOM,SS strategyStyle
    class CPP,PY,NB interfaceStyle
    class CM,GT,CI,DOC infraStyle
"""

# Create the mermaid diagram
png_path, svg_path = create_mermaid_diagram(
    diagram_code, 
    png_filepath='lob_backtester_architecture.png',
    svg_filepath='lob_backtester_architecture.svg',
    width=1400,
    height=1000
)

print(f"System architecture diagram saved as:")
print(f"PNG: {png_path}")
print(f"SVG: {svg_path}")