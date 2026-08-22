class CryptoManager {
    func decrypt(withKey key: String) -> String { return "plain-" + key }
}
let c = CryptoManager()
print(c.decrypt(withKey: "K"))
