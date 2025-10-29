# Agent Guidelines

* Early improvement is the source, but the performance-sensitive path (like WebRTC ＠输, C++ internal management, long-term control import line) is a clear improvement goal.
* If you have any bias, it may work differently on Windows/macOS/Linux or Clang/MSVC.
* Beginning and ending use **English style**.
* Half-width empty case used between full-width characters and half-width characters.
* Unnecessarily used facial expression code.
*Advanced thinking **As safe as possible, flexible and safe**.
*Ownership note, information used by the Japanese government **English-style English**.

---

## Comments

* Requires detailed explanation, prohibits redundant or imitation explanation.
* Preservation, elaboration, resolution **Cause but non-representation**.
* Review performance, bookmarks, and design details.
* The key area for performance (as in RTP/RTCP processing, editing, importing, etc.) is required.
* At the time of review, the following order: **Definitiveness > Performance > Flexibility > Flexibility**.
* Possessive English language with uniform usage, perfect phrase format for use.
* Unnecessary general text in the text section, written in the text section.
* Changing explanation (CHANGELOG) Must be submitted and maintained, and the reflection of the key technology must be maintained.

---

## Build

* Instructions for use: 

```bash 
python3 run.py build <target> 
````
* Guaranteed minimum CMake version and Python tools.
*Construction name: 

* `is_debug=false` 
* `is_component_build=false` 
* `use_rtti=true` 
* `rtc_use_h264=true` 
* `rtc_use_h265=true`
* Windows flatbed use `is_clang=true` given `use_lld=true`.
* Better choice of location and landscape adjustment (for example, low extension vs. high output).
* When the WebRTC model continues to increase, the ABI will remain unchanged during the demand clearing period.

---

## Version Control

* Compliant Git flow.
* Since the new feature function, `feature/` has been opened, and since the modification has been completed, `fix/` has been opened.
* Prohibit direct submission to main branch.
*Required communication information: 

* Use **English imperative expression** (as in *Add*, *Fix*, *Refactor*, *Improve*). 
* Compliance expression: 

```` 
<type>: <description> ~ <scope> 
```` 

Illustrative example: 

```` 
fix: resolve input desynchronization issue ~ remote_control 
```` 
* Evasion includes paste 词汇 (as *update*, *change*).
* Please check the previous page for the previous review.

---

## Testing

* Examination instruction: 

```bash 
uv run pytest -v 
````
* Uses Python 3.13 or higher version.
* Examination required cover: 

* WebRTC media flow line quality (encoding, transmission, decoding).
* In the distant import incident, the same-step and mouse-switch model (absolute/compatibility) were exchanged.
* Multi-wire level resource competition (lock, different steps, fixed time accuracy).
* Preferably used model environment running CI test (Windows/Linux/macOS).
* Ownership test results include demand and export of Japanese newspapers.