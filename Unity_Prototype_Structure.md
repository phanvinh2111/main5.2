# Unity Project Structure cho MU Mobile

## Project Setup

### 1. Unity Version & Settings
```
Unity Version: 2022.3 LTS
Platform: Android & iOS
Graphics API: OpenGL ES 3.0 / Metal
Target Devices: Android 7.0+, iOS 12.0+
```

### 2. Project Structure
```
Assets/
├── Scripts/
│   ├── Core/
│   │   ├── GameManager.cs
│   │   ├── DataManager.cs
│   │   ├── ResourceManager.cs
│   │   └── AudioManager.cs
│   ├── Network/
│   │   ├── NetworkManager.cs
│   │   ├── PacketProcessor.cs
│   │   ├── PacketHandler.cs
│   │   └── ConnectionManager.cs
│   ├── Game/
│   │   ├── Character/
│   │   │   ├── CharacterController.cs
│   │   │   ├── CharacterStats.cs
│   │   │   ├── CharacterAnimation.cs
│   │   │   └── CharacterEquipment.cs
│   │   ├── Combat/
│   │   │   ├── CombatSystem.cs
│   │   │   ├── SkillManager.cs
│   │   │   ├── DamageCalculator.cs
│   │   │   └── HitDetection.cs
│   │   ├── Inventory/
│   │   │   ├── InventoryManager.cs
│   │   │   ├── Item.cs
│   │   │   ├── Equipment.cs
│   │   │   └── ItemDatabase.cs
│   │   └── World/
│   │       ├── WorldManager.cs
│   │       ├── MapManager.cs
│   │       ├── MonsterManager.cs
│   │       └── NPCManager.cs
│   ├── UI/
│   │   ├── UIManager.cs
│   │   ├── MobileUI.cs
│   │   ├── VirtualJoystick.cs
│   │   ├── SkillButton.cs
│   │   ├── InventoryUI.cs
│   │   ├── CharacterUI.cs
│   │   └── ChatUI.cs
│   └── Utils/
│       ├── MathUtils.cs
│       ├── FileUtils.cs
│       ├── Encryption.cs
│       └── Logger.cs
├── Models/
│   ├── Characters/
│   ├── Monsters/
│   ├── Items/
│   └── Maps/
├── Textures/
│   ├── Characters/
│   ├── Monsters/
│   ├── Items/
│   ├── UI/
│   └── Maps/
├── Sounds/
│   ├── Music/
│   ├── SFX/
│   └── Voice/
├── Scenes/
│   ├── Login.unity
│   ├── CharacterSelect.unity
│   ├── Game.unity
│   └── Loading.unity
└── Resources/
    ├── Configs/
    ├── Localization/
    └── Data/
```

## Core Scripts Implementation

### 1. GameManager.cs
```csharp
using UnityEngine;
using System.Collections.Generic;

public class GameManager : MonoBehaviour
{
    public static GameManager Instance { get; private set; }
    
    [Header("Managers")]
    public NetworkManager networkManager;
    public UIManager uiManager;
    public AudioManager audioManager;
    public ResourceManager resourceManager;
    
    [Header("Game State")]
    public GameState currentState;
    public Character currentCharacter;
    
    private void Awake()
    {
        if (Instance == null)
        {
            Instance = this;
            DontDestroyOnLoad(gameObject);
            InitializeManagers();
        }
        else
        {
            Destroy(gameObject);
        }
    }
    
    private void InitializeManagers()
    {
        // Initialize all managers
        networkManager.Initialize();
        uiManager.Initialize();
        audioManager.Initialize();
        resourceManager.Initialize();
    }
    
    public void ChangeGameState(GameState newState)
    {
        currentState = newState;
        OnGameStateChanged(newState);
    }
    
    private void OnGameStateChanged(GameState state)
    {
        switch (state)
        {
            case GameState.Login:
                uiManager.ShowLoginUI();
                break;
            case GameState.CharacterSelect:
                uiManager.ShowCharacterSelectUI();
                break;
            case GameState.InGame:
                uiManager.ShowGameUI();
                break;
        }
    }
}

public enum GameState
{
    Login,
    CharacterSelect,
    InGame,
    Loading
}
```

### 2. NetworkManager.cs
```csharp
using UnityEngine;
using System;
using System.Net.Sockets;
using System.Threading.Tasks;

public class NetworkManager : MonoBehaviour
{
    [Header("Connection Settings")]
    public string serverIP = "127.0.0.1";
    public int serverPort = 44406;
    
    private TcpClient client;
    private NetworkStream stream;
    private PacketProcessor packetProcessor;
    private bool isConnected = false;
    
    public event Action<byte[]> OnPacketReceived;
    public event Action OnConnected;
    public event Action OnDisconnected;
    
    public void Initialize()
    {
        packetProcessor = new PacketProcessor();
        packetProcessor.OnPacketProcessed += HandlePacket;
    }
    
    public async Task<bool> ConnectToServer()
    {
        try
        {
            client = new TcpClient();
            await client.ConnectAsync(serverIP, serverPort);
            stream = client.GetStream();
            isConnected = true;
            
            OnConnected?.Invoke();
            
            // Start receiving packets
            _ = Task.Run(ReceivePackets);
            
            return true;
        }
        catch (Exception e)
        {
            Debug.LogError($"Failed to connect: {e.Message}");
            return false;
        }
    }
    
    public void SendPacket(byte[] data)
    {
        if (isConnected && stream != null)
        {
            try
            {
                stream.Write(data, 0, data.Length);
            }
            catch (Exception e)
            {
                Debug.LogError($"Failed to send packet: {e.Message}");
                Disconnect();
            }
        }
    }
    
    private async Task ReceivePackets()
    {
        byte[] buffer = new byte[4096];
        
        while (isConnected)
        {
            try
            {
                int bytesRead = await stream.ReadAsync(buffer, 0, buffer.Length);
                if (bytesRead > 0)
                {
                    byte[] packet = new byte[bytesRead];
                    Array.Copy(buffer, packet, bytesRead);
                    packetProcessor.ProcessPacket(packet);
                }
            }
            catch (Exception e)
            {
                Debug.LogError($"Error receiving packet: {e.Message}");
                break;
            }
        }
        
        Disconnect();
    }
    
    private void HandlePacket(byte[] packet)
    {
        OnPacketReceived?.Invoke(packet);
    }
    
    public void Disconnect()
    {
        isConnected = false;
        stream?.Close();
        client?.Close();
        OnDisconnected?.Invoke();
    }
}
```

### 3. CharacterController.cs
```csharp
using UnityEngine;
using System.Collections;

public class CharacterController : MonoBehaviour
{
    [Header("Character Data")]
    public Character character;
    public CharacterStats stats;
    
    [Header("Movement")]
    public float moveSpeed = 5f;
    public float rotationSpeed = 10f;
    public float jumpForce = 5f;
    
    [Header("Components")]
    public Animator animator;
    public Rigidbody rb;
    public CapsuleCollider collider;
    
    private Vector3 moveDirection;
    private bool isMoving = false;
    private bool isGrounded = true;
    
    private void Start()
    {
        InitializeCharacter();
    }
    
    private void InitializeCharacter()
    {
        // Load character model based on class
        LoadCharacterModel();
        
        // Setup animations
        SetupAnimations();
        
        // Apply stats
        ApplyStats();
    }
    
    public void Move(Vector3 direction)
    {
        moveDirection = direction.normalized;
        isMoving = direction.magnitude > 0.1f;
        
        if (isMoving)
        {
            // Rotate towards movement direction
            Quaternion targetRotation = Quaternion.LookRotation(moveDirection);
            transform.rotation = Quaternion.Lerp(transform.rotation, targetRotation, rotationSpeed * Time.deltaTime);
            
            // Move character
            Vector3 movement = moveDirection * moveSpeed * Time.deltaTime;
            transform.position += movement;
            
            // Update animation
            animator.SetBool("IsMoving", true);
            animator.SetFloat("MoveSpeed", moveDirection.magnitude);
        }
        else
        {
            animator.SetBool("IsMoving", false);
        }
    }
    
    public void Attack(int skillId)
    {
        if (CanUseSkill(skillId))
        {
            StartCoroutine(PerformAttack(skillId));
        }
    }
    
    private IEnumerator PerformAttack(int skillId)
    {
        // Play attack animation
        animator.SetTrigger("Attack");
        animator.SetInteger("SkillID", skillId);
        
        // Wait for animation
        yield return new WaitForSeconds(0.5f);
        
        // Calculate damage and apply
        int damage = CalculateDamage(skillId);
        // Apply damage to target
        
        // Send attack packet to server
        SendAttackPacket(skillId, damage);
    }
    
    private bool CanUseSkill(int skillId)
    {
        // Check if skill is available and has enough mana
        return stats.currentMana >= GetSkillManaCost(skillId);
    }
    
    private int CalculateDamage(int skillId)
    {
        // Calculate damage based on stats, equipment, and skill
        float baseDamage = stats.attackDamage;
        float skillMultiplier = GetSkillDamageMultiplier(skillId);
        float criticalChance = stats.criticalRate;
        
        float damage = baseDamage * skillMultiplier;
        
        // Apply critical hit
        if (Random.Range(0f, 1f) < criticalChance)
        {
            damage *= 2f;
        }
        
        return Mathf.RoundToInt(damage);
    }
    
    private void LoadCharacterModel()
    {
        // Load character model based on class and skin
        string modelPath = $"Characters/{character.Class}_{character.Skin}";
        GameObject modelPrefab = Resources.Load<GameObject>(modelPath);
        
        if (modelPrefab != null)
        {
            GameObject modelInstance = Instantiate(modelPrefab, transform);
            animator = modelInstance.GetComponent<Animator>();
        }
    }
    
    private void SetupAnimations()
    {
        if (animator != null)
        {
            // Setup animation parameters
            animator.SetBool("IsMoving", false);
            animator.SetFloat("MoveSpeed", 0f);
        }
    }
    
    private void ApplyStats()
    {
        // Apply character stats to movement and combat
        moveSpeed = 5f + (stats.dexterity * 0.1f);
        jumpForce = 5f + (stats.strength * 0.05f);
    }
    
    private void SendAttackPacket(int skillId, int damage)
    {
        // Create and send attack packet
        byte[] packet = CreateAttackPacket(skillId, damage);
        NetworkManager.Instance.SendPacket(packet);
    }
    
    private byte[] CreateAttackPacket(int skillId, int damage)
    {
        // Create binary packet for attack
        // This would match the original C++ packet format
        return new byte[0]; // Placeholder
    }
}
```

### 4. MobileUI.cs
```csharp
using UnityEngine;
using UnityEngine.UI;

public class MobileUI : MonoBehaviour
{
    [Header("Movement Controls")]
    public VirtualJoystick movementJoystick;
    public Button jumpButton;
    
    [Header("Combat Controls")]
    public SkillButton[] skillButtons;
    public Button attackButton;
    public Button targetButton;
    
    [Header("UI Panels")]
    public GameObject gamePanel;
    public GameObject inventoryPanel;
    public GameObject characterPanel;
    public GameObject chatPanel;
    public GameObject menuPanel;
    
    [Header("UI Elements")]
    public Text healthText;
    public Text manaText;
    public Text levelText;
    public Text experienceText;
    public Slider healthBar;
    public Slider manaBar;
    public Slider experienceBar;
    
    private CharacterController playerController;
    private bool isInventoryOpen = false;
    private bool isCharacterOpen = false;
    
    private void Start()
    {
        InitializeUI();
        SetupTouchControls();
    }
    
    private void InitializeUI()
    {
        // Setup joystick
        if (movementJoystick != null)
        {
            movementJoystick.OnJoystickMoved += OnJoystickMoved;
        }
        
        // Setup skill buttons
        for (int i = 0; i < skillButtons.Length; i++)
        {
            int skillIndex = i; // Capture for lambda
            skillButtons[i].OnButtonPressed += () => OnSkillButtonPressed(skillIndex);
        }
        
        // Setup other buttons
        if (attackButton != null)
            attackButton.onClick.AddListener(OnAttackButtonPressed);
        
        if (jumpButton != null)
            jumpButton.onClick.AddListener(OnJumpButtonPressed);
    }
    
    private void SetupTouchControls()
    {
        // Setup gesture recognition
        // This would include pinch, swipe, tap gestures
    }
    
    private void OnJoystickMoved(Vector2 direction)
    {
        if (playerController != null)
        {
            Vector3 moveDirection = new Vector3(direction.x, 0, direction.y);
            playerController.Move(moveDirection);
        }
    }
    
    private void OnSkillButtonPressed(int skillIndex)
    {
        if (playerController != null)
        {
            playerController.Attack(skillIndex);
        }
    }
    
    private void OnAttackButtonPressed()
    {
        if (playerController != null)
        {
            playerController.Attack(0); // Basic attack
        }
    }
    
    private void OnJumpButtonPressed()
    {
        if (playerController != null)
        {
            // Trigger jump
        }
    }
    
    public void ToggleInventory()
    {
        isInventoryOpen = !isInventoryOpen;
        inventoryPanel.SetActive(isInventoryOpen);
    }
    
    public void ToggleCharacter()
    {
        isCharacterOpen = !isCharacterOpen;
        characterPanel.SetActive(isCharacterOpen);
    }
    
    public void UpdateUI(Character character)
    {
        if (character != null)
        {
            healthText.text = $"{character.CurrentHealth}/{character.MaxHealth}";
            manaText.text = $"{character.CurrentMana}/{character.MaxMana}";
            levelText.text = $"Level {character.Level}";
            experienceText.text = $"{character.Experience}/{character.NextExperience}";
            
            healthBar.value = (float)character.CurrentHealth / character.MaxHealth;
            manaBar.value = (float)character.CurrentMana / character.MaxMana;
            experienceBar.value = (float)character.Experience / character.NextExperience;
        }
    }
    
    public void ShowChat()
    {
        chatPanel.SetActive(true);
    }
    
    public void HideChat()
    {
        chatPanel.SetActive(false);
    }
    
    public void ShowMenu()
    {
        menuPanel.SetActive(true);
    }
    
    public void HideMenu()
    {
        menuPanel.SetActive(false);
    }
}
```

### 5. VirtualJoystick.cs
```csharp
using UnityEngine;
using UnityEngine.UI;
using UnityEngine.EventSystems;
using System;

public class VirtualJoystick : MonoBehaviour, IPointerDownHandler, IDragHandler, IPointerUpHandler
{
    [Header("Joystick Settings")]
    public float joystickRadius = 50f;
    public bool snapToCenter = true;
    
    [Header("UI References")]
    public RectTransform joystickBackground;
    public RectTransform joystickHandle;
    
    public event Action<Vector2> OnJoystickMoved;
    public event Action OnJoystickReleased;
    
    private Vector2 joystickCenter;
    private Vector2 joystickDirection;
    private bool isDragging = false;
    
    private void Start()
    {
        if (joystickBackground != null)
        {
            joystickCenter = joystickBackground.anchoredPosition;
        }
        
        ResetJoystick();
    }
    
    public void OnPointerDown(PointerEventData eventData)
    {
        isDragging = true;
        OnDrag(eventData);
    }
    
    public void OnDrag(PointerEventData eventData)
    {
        if (!isDragging) return;
        
        Vector2 touchPosition;
        RectTransformUtility.ScreenPointToLocalPointInRectangle(
            joystickBackground, eventData.position, eventData.pressEventCamera, out touchPosition);
        
        joystickDirection = (touchPosition - joystickCenter).normalized;
        
        float distance = Vector2.Distance(touchPosition, joystickCenter);
        if (distance > joystickRadius)
        {
            joystickDirection *= joystickRadius;
        }
        else
        {
            joystickDirection = touchPosition - joystickCenter;
        }
        
        UpdateJoystickVisual();
        OnJoystickMoved?.Invoke(joystickDirection.normalized);
    }
    
    public void OnPointerUp(PointerEventData eventData)
    {
        isDragging = false;
        ResetJoystick();
        OnJoystickReleased?.Invoke();
    }
    
    private void UpdateJoystickVisual()
    {
        if (joystickHandle != null)
        {
            joystickHandle.anchoredPosition = joystickCenter + joystickDirection;
        }
    }
    
    private void ResetJoystick()
    {
        joystickDirection = Vector2.zero;
        UpdateJoystickVisual();
    }
    
    public Vector2 GetDirection()
    {
        return joystickDirection.normalized;
    }
    
    public float GetMagnitude()
    {
        return joystickDirection.magnitude / joystickRadius;
    }
}
```

### 6. SkillButton.cs
```csharp
using UnityEngine;
using UnityEngine.UI;
using System;

public class SkillButton : MonoBehaviour
{
    [Header("Skill Settings")]
    public int skillId;
    public float cooldown = 5f;
    
    [Header("UI References")]
    public Image skillIcon;
    public Image cooldownOverlay;
    public Text cooldownText;
    public Button button;
    
    public event Action OnButtonPressed;
    
    private float currentCooldown = 0f;
    private bool isOnCooldown = false;
    
    private void Start()
    {
        if (button != null)
        {
            button.onClick.AddListener(OnButtonClick);
        }
        
        LoadSkillIcon();
    }
    
    private void Update()
    {
        if (isOnCooldown)
        {
            UpdateCooldown();
        }
    }
    
    private void OnButtonClick()
    {
        if (!isOnCooldown)
        {
            OnButtonPressed?.Invoke();
            StartCooldown();
        }
    }
    
    private void StartCooldown()
    {
        currentCooldown = cooldown;
        isOnCooldown = true;
        button.interactable = false;
        
        if (cooldownOverlay != null)
        {
            cooldownOverlay.gameObject.SetActive(true);
        }
    }
    
    private void UpdateCooldown()
    {
        currentCooldown -= Time.deltaTime;
        
        if (cooldownOverlay != null)
        {
            cooldownOverlay.fillAmount = currentCooldown / cooldown;
        }
        
        if (cooldownText != null)
        {
            cooldownText.text = Mathf.CeilToInt(currentCooldown).ToString();
        }
        
        if (currentCooldown <= 0f)
        {
            EndCooldown();
        }
    }
    
    private void EndCooldown()
    {
        isOnCooldown = false;
        button.interactable = true;
        
        if (cooldownOverlay != null)
        {
            cooldownOverlay.gameObject.SetActive(false);
        }
        
        if (cooldownText != null)
        {
            cooldownText.text = "";
        }
    }
    
    private void LoadSkillIcon()
    {
        if (skillIcon != null)
        {
            string iconPath = $"UI/Skills/skill_{skillId}";
            Sprite iconSprite = Resources.Load<Sprite>(iconPath);
            
            if (iconSprite != null)
            {
                skillIcon.sprite = iconSprite;
            }
        }
    }
    
    public void SetSkill(int newSkillId)
    {
        skillId = newSkillId;
        LoadSkillIcon();
    }
    
    public void SetCooldown(float newCooldown)
    {
        cooldown = newCooldown;
    }
}
```

## Build Settings

### Android Settings
```
Player Settings:
- Company Name: Your Company
- Product Name: MU Mobile
- Package Name: com.yourcompany.mumobile
- Minimum API Level: Android 7.0 (API level 24)
- Target API Level: Android 13.0 (API level 33)
- Scripting Backend: IL2CPP
- Target Architectures: ARM64

Graphics:
- Graphics APIs: OpenGL ES 3.0
- Multithreaded Rendering: Enabled
- Static Batching: Enabled
- Dynamic Batching: Enabled

Quality:
- Quality Level: Medium (for performance)
- Anti Aliasing: 2x Multi Sampling
- Texture Quality: Full Res
- Anisotropic Textures: Per Texture
```

### iOS Settings
```
Player Settings:
- Company Name: Your Company
- Product Name: MU Mobile
- Bundle Identifier: com.yourcompany.mumobile
- Target minimum iOS Version: 12.0
- Architecture: ARM64
- Scripting Backend: IL2CPP

Graphics:
- Graphics APIs: Metal
- Multithreaded Rendering: Enabled
- Static Batching: Enabled
- Dynamic Batching: Enabled

Quality:
- Quality Level: Medium
- Anti Aliasing: 2x Multi Sampling
- Texture Quality: Full Res
- Anisotropic Textures: Per Texture
```

## Performance Optimization

### 1. Graphics Optimization
```csharp
// LOD System
public class LODManager : MonoBehaviour
{
    public float[] lodDistances = { 10f, 30f, 100f };
    public GameObject[] lodLevels;
    
    private void Update()
    {
        float distance = Vector3.Distance(transform.position, Camera.main.transform.position);
        
        for (int i = 0; i < lodDistances.Length; i++)
        {
            if (distance <= lodDistances[i])
            {
                SetLODLevel(i);
                break;
            }
        }
    }
    
    private void SetLODLevel(int level)
    {
        for (int i = 0; i < lodLevels.Length; i++)
        {
            lodLevels[i].SetActive(i == level);
        }
    }
}
```

### 2. Memory Management
```csharp
// Object Pooling
public class ObjectPool : MonoBehaviour
{
    public GameObject prefab;
    public int poolSize = 20;
    
    private Queue<GameObject> objectPool;
    
    private void Start()
    {
        objectPool = new Queue<GameObject>();
        
        for (int i = 0; i < poolSize; i++)
        {
            GameObject obj = Instantiate(prefab);
            obj.SetActive(false);
            objectPool.Enqueue(obj);
        }
    }
    
    public GameObject GetObject()
    {
        if (objectPool.Count > 0)
        {
            GameObject obj = objectPool.Dequeue();
            obj.SetActive(true);
            return obj;
        }
        
        return Instantiate(prefab);
    }
    
    public void ReturnObject(GameObject obj)
    {
        obj.SetActive(false);
        objectPool.Enqueue(obj);
    }
}
```

### 3. Network Optimization
```csharp
// Packet Compression
public class PacketCompressor
{
    public static byte[] Compress(byte[] data)
    {
        // Use LZ4 or similar compression
        return data; // Placeholder
    }
    
    public static byte[] Decompress(byte[] data)
    {
        // Decompress data
        return data; // Placeholder
    }
}
```

## Testing Strategy

### 1. Unit Tests
```csharp
using NUnit.Framework;

[TestFixture]
public class CharacterControllerTests
{
    [Test]
    public void Move_ValidDirection_CharacterMoves()
    {
        // Arrange
        var character = new CharacterController();
        Vector3 direction = Vector3.forward;
        
        // Act
        character.Move(direction);
        
        // Assert
        Assert.IsTrue(character.IsMoving);
    }
    
    [Test]
    public void Attack_ValidSkill_AttackPerformed()
    {
        // Arrange
        var character = new CharacterController();
        int skillId = 1;
        
        // Act
        character.Attack(skillId);
        
        // Assert
        Assert.IsTrue(character.IsAttacking);
    }
}
```

### 2. Performance Tests
```csharp
public class PerformanceMonitor : MonoBehaviour
{
    public float targetFPS = 60f;
    public float currentFPS;
    
    private void Update()
    {
        currentFPS = 1f / Time.deltaTime;
        
        if (currentFPS < targetFPS * 0.8f)
        {
            Debug.LogWarning($"Low FPS detected: {currentFPS}");
            OptimizePerformance();
        }
    }
    
    private void OptimizePerformance()
    {
        // Reduce quality settings
        // Disable unnecessary effects
        // Reduce draw distance
    }
}
```

## Deployment Checklist

### Pre-Build
- [ ] All assets optimized for mobile
- [ ] Network protocol tested
- [ ] UI tested on different screen sizes
- [ ] Performance benchmarks met
- [ ] Memory usage within limits

### Build
- [ ] Android APK/AAB generated
- [ ] iOS Xcode project generated
- [ ] Signing certificates configured
- [ ] App icons and splash screens added

### Post-Build
- [ ] App tested on multiple devices
- [ ] Network connectivity tested
- [ ] Performance validated
- [ ] Crash reporting configured
- [ ] Analytics integrated

## Conclusion

This Unity project structure provides a solid foundation for porting MU Online to mobile. The modular design allows for easy maintenance and expansion, while the performance optimizations ensure smooth gameplay on mobile devices.

Key benefits of this approach:
1. **Cross-platform compatibility** with single codebase
2. **Modular architecture** for easy development
3. **Performance optimized** for mobile devices
4. **Scalable design** for future features
5. **Comprehensive testing** strategy

The next step would be to implement the actual game logic and integrate with the existing MU Online server infrastructure.