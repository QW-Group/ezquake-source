# Antilag 1 client scope

QW-Group/ezQuake already carries the Antilag 1 prototype protocol work.  This
branch tracks its validation and any small compatibility fixes required by the
clean QW-Group/KTX and QW-Group/MVDSV ports.

The client scope is limited to the existing Antilag 1 protocol behaviour:
accurate timings, weapon prediction and simple projectiles.  It contains no
player-spray or decal functionality.  Sprays are not a client-side dependency
of Antilag 1 and must remain outside this series.
