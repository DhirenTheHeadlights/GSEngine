# Lucky Robots Interview Prep - Q&A

## Opening / Background

**Q: Tell me about yourself.**

A: "I'm a C++ developer focused on game engine architecture. For the past few years I've been building GSEngine—a custom 3D game engine from scratch in C++20 with a Vulkan renderer, physics simulation, and multi-threaded task system. I built it because I wanted to understand engine internals deeply, not just use existing tools. When I saw Lucky Robots is building a simulation engine for robotics, it immediately resonated—deterministic physics and real-time performance are exactly what I've been working on."

---

**Q: Walk me through your engine. What does it do?**

A: "GSEngine is a modular game engine with four main pillars. First, a Vulkan-based deferred renderer with G-buffer, shadow mapping, and async resource loading. Second, a physics system with custom integrators for gravity, friction, air resistance, and torque—all using strongly-typed units like meters and seconds to prevent unit errors. Third, a multi-threaded task system built on Intel TBB that automatically detects write conflicts between systems and schedules them into safe parallel phases. Fourth, a double-buffered entity-component system that lets me read last frame's state while writing the current frame, which avoids race conditions without heavy locking."

---

**Q: What's the hardest technical problem you solved in this project?**

A: "Getting the task scheduler right. I needed systems to run in parallel, but some write to the same components. My first approach was manual dependency declaration, which was error-prone. I rewrote it to automatically track which component types each task writes to, then build execution phases where no two tasks in the same phase conflict. The tricky part was making this detection happen at task-queue time without runtime overhead—I used type indices and a greedy phase-building algorithm. Now I can add new systems without manually managing dependencies."

---

**Q: Why did you choose Vulkan over OpenGL or another API?**

A: "Two reasons. First, I wanted explicit control over GPU resources—Vulkan forces you to understand command buffers, synchronization, and memory allocation, which is exactly what I wanted to learn. Second, Vulkan's model maps better to how modern GPUs actually work, so the skills transfer to other low-level APIs like D3D12 or Metal. The upfront complexity is high, but it pays off in performance control."

---

**Q: Why C++20 specifically? What features do you use?**

A: "Modules are the big one—they dramatically improve compile times and enforce cleaner architecture since you can't have circular dependencies. I also use concepts for constraining templates, std::span for non-owning views into arrays, and structured bindings throughout. The strongly-typed unit system in my physics code relies heavily on template metaprogramming and concepts to catch unit errors at compile time."

---

## Technical Deep-Dives

**Q: How does your physics simulation ensure determinism?**

A: "Three things. First, fixed timestep—the physics update always advances by the same delta, regardless of frame rate. Second, strongly-typed units prevent accidental mixing of meters and seconds or other unit errors that would cause drift. Third, the integration order is deterministic—I update gravity, then air resistance, then velocity, then position, then rotation, in that exact order every frame. Quaternion integration uses the standard 0.5 * omega * q formula to avoid gimbal lock and maintain orientation accuracy."

---

**Q: Explain your collision detection approach.**

A: "Two phases. Broad phase uses AABB overlap tests—cheap and fast to reject pairs that can't possibly collide. I also do swept AABB for moving objects to catch tunneling. Narrow phase handles the actual collision resolution—computing penetration depth, contact normals, and applying position corrections and velocity responses. The broad phase runs first to minimize expensive narrow phase calls."

---

**Q: How does your ECS handle thread safety?**

A: "Double buffering. The registry maintains two copies of component state—read and write. During update, systems read from last frame's buffer and write to the current frame's buffer. At frame end, the buffers swap. This means readers never see partially-written data, and I don't need fine-grained locks on every component access. The tradeoff is memory—I'm storing two copies—but for real-time performance the latency predictability is worth it."

---

**Q: What would you do differently if you started over?**

A: "I'd design the resource loading system earlier with async in mind from day one. I retrofitted it, and there are some awkward seams where synchronous assumptions leaked through. I'd also consider a more formal job graph instead of my current phase-based scheduler—something like Taskflow or a DAG-based approach would make complex dependencies cleaner."

---

## Motivation / Fit

**Q: Why Lucky Robots? Why robotics simulation?**

A: "Game engines have solved incredible real-time simulation problems, but games can cheat—if physics is slightly wrong, players don't notice. Robotics can't cheat. The requirement for determinism and physical accuracy makes it a harder, more interesting problem. I'm also drawn to the impact—training robots in simulation at scale could accelerate robotics development dramatically. And honestly, your job posting description about 'would the USB output be identical' is exactly how I think about engine development."

---

**Q: This is an early-stage startup. How do you feel about ambiguity and fast iteration?**

A: "That's how I work on GSEngine. I don't have a product manager telling me what to build—I identify problems, prototype solutions, throw away what doesn't work, and iterate. I'm comfortable making architectural decisions without perfect information and refactoring when requirements change. I'd rather ship something and learn than over-plan."

---

**Q: Where do you see gaps in your experience?**

A: "I haven't worked on a production robotics simulation specifically—my physics is game-oriented, not URDF-based robot models or ROS integration. But the core skills transfer: rigid body dynamics, collision detection, deterministic integration. I'd need to learn the robotics-specific domain, but the engine fundamentals are solid."

---

**Q: You don't have much traditional industry experience. Why should we take a chance on you?**

A: "GSEngine isn't a tutorial project—it's thousands of hours of real architecture work. I've made the mistakes you make building production systems: bad abstractions that had to be ripped out, race conditions that only showed up under load, performance cliffs from unexpected cache misses. The difference between me and someone with five years at a game studio is I made those mistakes on my own project instead of theirs. The learning is the same."

---

## Questions to Ask Them

- "What does the current engine architecture look like? Is it Unreal-based, Unity-based, or fully custom?"
- "What's the hardest simulation fidelity problem you're working on right now?"
- "How do you validate that sim behavior matches real-world robot behavior?"
- "What would success look like for this role in the first 90 days?"
- "How big is the engineering team currently, and how do you see it growing?"
- "What's the balance between new feature development vs. performance optimization?"

---

## Quick Stats to Have Ready

- **Engine name**: GSEngine
- **Language**: C++20 with modules
- **Renderer**: Vulkan, deferred shading, G-buffer
- **Physics**: Custom integrators, typed units, broad/narrow phase collision
- **Threading**: Intel TBB, conflict-detecting scheduler
- **ECS**: Double-buffered registry with typed channels
- **GitHub**: DhirenTheHeadlights
